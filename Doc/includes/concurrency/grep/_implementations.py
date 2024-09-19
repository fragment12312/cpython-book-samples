import contextlib
import re

# For concurrency:
import asyncio
import concurrent.futures
import test.support.interpreters as interpreters
import test.support.interpreters.queues
import multiprocessing
import queue
import threading

from . import search_lines, Match, Options
from ._utils import iter_lines


multiprocessing.set_start_method('spawn')


#######################################
# basic search implementations

# [start-impl-sequential]
def search_sequential(filenames, regex, opts):
    for filename in filenames:
        # iter_lines() opens the file too.
        lines = iter_lines(filename)
        yield from search_lines(lines, regex, opts, filename)
# [end-impl-sequential]


# [start-impl-threads-basic]
def search_threads_basic(filenames, regex, opts):
    matches_by_file = queue.Queue()

    def do_background():
        MAX_FILES = 10
        MAX_MATCHES = 100

        # Make sure we don't have too many threads at once,
        # i.e. too many files open at once.
        counter = threading.Semaphore(MAX_FILES)

        def task(filename, matches):
            lines = iter_lines(filename)
            for match in search_lines(lines, regex, opts, filename):
                matches.put(match)  # blocking
            matches.put(None)  # blocking
            # Let a new thread start.
            counter.release()

        for filename in filenames:
            # Prepare for the file.
            matches = queue.Queue(MAX_MATCHES)
            matches_by_file.put(matches)

            # Start a thread to process the file.
            t = threading.Thread(target=task, args=(filename, matches))
            counter.acquire()
            t.start()
        matches_by_file.put(None)

    background = threading.Thread(target=do_background)
    background.start()

    # Yield the results as they are received, in order.
    matches = matches_by_file.get()  # blocking
    while matches is not None:
        match = matches.get()  # blocking
        while match is not None:
            yield match
            match = matches.get()  # blocking
        matches = matches_by_file.get()  # blocking

    background.join()
# [end-impl-threads-basic]


# [start-impl-cf-threads-basic]
def search_cf_threads_basic(filenames, regex, opts):
    matches_by_file = queue.Queue()

    def do_background():
        MAX_FILES = 10
        MAX_MATCHES = 100

        def task(filename, matches):
            lines = iter_lines(filename)
            for match in search_lines(lines, regex, opts, filename):
                matches.put(match)  # blocking
            matches.put(None)  # blocking

        with concurrent.futures.ThreadPoolExecutor(MAX_FILES) as workers:
            for filename in filenames:
                # Prepare for the file.
                matches = queue.Queue(MAX_MATCHES)
                matches_by_file.put((filename, matches))

                # Start a thread to process the file.
                workers.submit(task, filename, matches)
            matches_by_file.put(None)

    background = threading.Thread(target=do_background)
    background.start()

    # Yield the results as they are received, in order.
    next_matches = matches_by_file.get()  # blocking
    while next_matches is not None:
        filename, matches = next_matches
        match = matches.get()  # blocking
        while match is not None:
            yield match
            match = matches.get()  # blocking
        next_matches = matches_by_file.get()  # blocking

    background.join()
# [end-impl-cf-threads-basic]


# [start-impl-interpreters-basic]
def _interpreter_prep(regex_pat, regex_flags, opts):
    regex = re.compile(regex_pat, regex_flags)
    opts = Options(**dict(opts))

    def task(filename, matches):
        lines = iter_lines(filename)
        for match in search_lines(lines, regex, opts, filename):
            matches.put(match)  # blocking
        matches.put(None)  # blocking
    return task


def search_interpreters_basic(filenames, regex, opts):
    matches_by_file = interpreters.queues.create()

    def do_background():
        MAX_FILES = 10
        MAX_MATCHES = 100

        def new_interpreter():
            interp = interpreters.create()
            interp.exec(f"""if True:
                import grep._implementations
                task = grep._implementations._interpreter_prep(
                    {regex.pattern!r},
                    {regex.flags},
                    {tuple(vars(opts).items())},
                )
                """)
            return interp

        ready_workers = queue.Queue(MAX_FILES)
        workers = []

        def next_worker():
            if len(workers) < MAX_FILES:
                interp = new_interpreter()
                workers.append(interp)
            ready_workers.put(interp)
            return ready_workers.get()  # blocking

        def do_work(filename, matches, interp):
            #interp.call(task, (regex, opts, filename, matches))
            interp.prepare_main(matches=matches)
            interp.exec(f'task({filename!r}, matches)')
            # Let a new thread start.
            ready_workers.put(interp)

        for filename in filenames:
            # Prepare for the file.
            matches = interpreters.queues.create(MAX_MATCHES)
            matches_by_file.put((filename, matches))
            interp = next_worker()

            # Start a thread to process the file.
            t = threading.Thread(
                target=do_work,
                args=(filename, matches, interp),
            )
            t.start()
        matches_by_file.put(None)

    background = threading.Thread(target=do_background)
    background.start()

    # Yield the results as they are received, in order.
    next_matches = matches_by_file.get()  # blocking
    while next_matches is not None:
        filename, matches = next_matches
        match = matches.get()  # blocking
        while match is not None:
            yield match
            match = matches.get()  # blocking
        next_matches = matches_by_file.get()  # blocking

    background.join()
# [end-impl-interpreters-basic]


# [start-impl-cf-interpreters-basic]
def search_cf_interpreters_basic(filenames, regex, opts):
    # We don't have this executor yet.
    raise NotImplementedError
# [end-impl-cf-interpreters-basic]


# [start-impl-asyncio-basic]
async def search_asyncio_basic(filenames, regex, opts):
    raise NotImplementedError


def search_asyncio_basic(filenames, regex, opts):
    raise NotImplementedError
# [end-impl-asyncio-basic]


# [start-impl-multiprocessing-basic]
def search_multiprocessing_basic(filenames, regex, opts):
    matches_by_file = queue.Queue()

    def do_background():
        MAX_FILES = 10
        MAX_MATCHES = 100

        # Make sure we don't have too many threads at once,
        # i.e. too many files open at once.
        counter = threading.Semaphore(MAX_FILES)
        finished = multiprocessing.Queue()
        active = {}
        done = False

        def monitor_tasks():
            while not done:
                try:
                    index = finished.get(timeout=0.1)
                except queue.Empty:
                    continue
                proc = active.pop(index)
                proc.join(0.1)
                if proc.is_alive():
                    # It's taking too long to terminate.
                    # We can wait for it at the end.
                    active[index] = proc
                # Let a new thread start.
                counter.release()
        monitor = threading.Thread(target=monitor_tasks)
        monitor.start()

        def task(filename, matches, index, finished):
            lines = iter_lines(filename)
            for match in search_lines(lines, regex, opts, filename):
                matches.put(match)  # blocking
            matches.put(None)  # blocking
            # Let a new thread start.
            #counter.release()
            finished.put(index)

        for index, filename in enumerate(filenames):
            # Prepare for the file.
            matches = multiprocessing.Queue(MAX_MATCHES)
            matches_by_file.put(matches)

            # Start a subprocess to process the file.
            proc = multiprocessing.Process(
                target=task,
                args=(filename, matches, index, finished),
            )
            counter.acquire(blocking=True)
            active[index] = proc
            proc.start()
        matches_by_file.put(None)
        # Wait for all remaining tasks to finish.
        done = True
        monitor.join()
        for proc in active.values():
            proc.join()

    background = threading.Thread(target=do_background)
    background.start()

    # Yield the results as they are received, in order.
    matches = matches_by_file.get()  # blocking
    while matches is not None:
        match = matches.get()  # blocking
        while match is not None:
            yield match
            match = matches.get()  # blocking
        matches = matches_by_file.get()  # blocking

    background.join()
# [end-impl-multiprocessing-basic]


# [start-impl-cf-multiprocessing-basic]
def search_cf_multiprocessing_basic(filenames, regex, opts):
    raise NotImplementedError
    matches_by_file = queue.Queue()

    def do_background():
        MAX_FILES = 10
        MAX_MATCHES = 100

        def task(filename, matches, regex, opts):
            lines = iter_lines(filename)
            for match in search_lines(lines, regex, opts, filename):
                matches.put(match)  # blocking
            matches.put(None)  # blocking

        with concurrent.futures.ProcessPoolExecutor(MAX_FILES) as workers:
            for filename in filenames:
                # Prepare for the file.
                matches = multiprocessing.Queue(MAX_MATCHES)
                matches_by_file.put(matches)

                # Start a thread to process the file.
                workers.submit(task, filename, matches, regex, opts)
            matches_by_file.put(None)

    background = threading.Thread(target=do_background)
    background.start()

    # Yield the results as they are received, in order.
    matches = matches_by_file.get()  # blocking
    while matches is not None:
        match = matches.get()  # blocking
        while match is not None:
            yield match
            match = matches.get()  # blocking
        matches = matches_by_file.get()  # blocking

    background.join()
# [end-impl-cf-multiprocessing-basic]


#######################################
# registered implementations

IMPLEMENTATIONS = {
    'sequential': {
        'basic': search_sequential,
    },
    'threads': {
        'basic': search_threads_basic,
    },
    'interpreters': {
        'basic': search_interpreters_basic,
    },
    'asyncio': {
        'basic': search_asyncio_basic,
    },
    'multiprocessing': {
        'basic': search_multiprocessing_basic,
    },
}
CF_IMPLEMENTATIONS = {
    'threads': {
        'basic': search_cf_threads_basic,
    },
    'interpreters': {
        'basic': search_cf_interpreters_basic,
    },
    'multiprocessing': {
        'basic': search_cf_multiprocessing_basic,
    },
}


def impl_from_name(name, kind=None, cf=False):
    if cf is False or cf is None:
        registry = IMPLEMENTATIONS
    else:
        if cf is not True:
            kind = cf
        registry = CF_IMPLEMENTATIONS
    if not kind:
        name, _, kind = name.partition(':')
    elif ':' in name:
        raise ValueError(f'got kind arg and kind in name {name!r}')
    try:
        impls = registry[name]
    except KeyError:
        if cf and name in IMPLEMENTATIONS:
            raise ValueError(f'impl {name!r} not supported by concurrent.futures')
        raise ValueError(f'unsupported impl name {name!r}')
    try:
        return impls[kind or 'basic']
    except KeyError:
        raise ValueError(f'unsupported impl kind {kind!r} for name {name!r}')


class ImplWrapper:
    def __init__(self, impl):
        assert not hasattr(type(impl), '__enter__'), impl
        assert callable(impl), impl
        self._impl = impl
    def __enter__(self):
        return self._impl
    def __exit__(self, *args):
        pass
    def __call__(self, filenames, regex, opts):
        return self._impl(filenames, regex, opts)


def resolve_impl(impl, kind=None, cf=None):
    if isinstance(impl, str):
        impl = impl_from_name(impl, kind, cf)
    else:
        assert not kind, kind
        assert not cf, cf
        if impl is None:
            impl = search_sequential

    if callable(impl):
        if not hasattr(type(impl), '__enter__'):
            impl = ImplWrapper(impl)
        return impl
    else:
        raise TypeError(impl)


# [start-do-search]
def resolve_search(impl, kind=None, cf=None):
    # "impl" is a callable that performs a search for
    # a set of files.  That might be a function or not.
    impl = resolve_impl(impl, kind, cf)
    assert hasattr(type(impl), '__enter__'), impl

    @contextlib.contextmanager
    def do_search(filenames, regex, opts):
        # If not a function, tt may involve resources that
        # need to be cleaned up when we're done, in which
        # case it can be used as a context manager.
        with impl as search:

            # "maches" is an iteraterable of Match objects.
            # Concurrency can only happen here.
            matches = search(filenames, regex, opts)

            # Just like with "impl" this search operation may
            # involve resources that need to be cleaned up
            # once we're done searching.  Thus the returned
            # object might also be a context manager.
            cm = matches
            if not hasattr(type(cm), '__enter__'):
                cm = contextlib.nullcontext(matches)
            with cm as matches:

                # This is what do_search() actually "returns".
                yield matches
    return do_search
# [end-do-search]
