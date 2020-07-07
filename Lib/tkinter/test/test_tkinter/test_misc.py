import unittest
import tkinter
import threading, time
import time
from test import support
from tkinter.test.support import AbstractTkTest

support.requires('gui')

class MiscTest(AbstractTkTest, unittest.TestCase):

    def test_all(self):
        self.assertIn("Widget", tkinter.__all__)
        # Check that variables from tkinter.constants are also in tkinter.__all__
        self.assertIn("CASCADE", tkinter.__all__)
        self.assertIsNotNone(tkinter.CASCADE)
        # Check that sys, re, and constants are not in tkinter.__all__
        self.assertNotIn("re", tkinter.__all__)
        self.assertNotIn("sys", tkinter.__all__)
        self.assertNotIn("constants", tkinter.__all__)
        # Check that an underscored functions is not in tkinter.__all__
        self.assertNotIn("_tkerror", tkinter.__all__)
        # Check that wantobjects is not in tkinter.__all__
        self.assertNotIn("wantobjects", tkinter.__all__)

    def test_repr(self):
        t = tkinter.Toplevel(self.root, name='top')
        f = tkinter.Frame(t, name='child')
        self.assertEqual(repr(f), '<tkinter.Frame object .top.child>')

    def test_generated_names(self):
        t = tkinter.Toplevel(self.root)
        f = tkinter.Frame(t)
        f2 = tkinter.Frame(t)
        b = tkinter.Button(f2)
        for name in str(b).split('.'):
            self.assertFalse(name.isidentifier(), msg=repr(name))

    def test_tk_setPalette(self):
        root = self.root
        root.tk_setPalette('black')
        self.assertEqual(root['background'], 'black')
        root.tk_setPalette('white')
        self.assertEqual(root['background'], 'white')
        self.assertRaisesRegex(tkinter.TclError,
                '^unknown color name "spam"$',
                root.tk_setPalette, 'spam')

        root.tk_setPalette(background='black')
        self.assertEqual(root['background'], 'black')
        root.tk_setPalette(background='blue', highlightColor='yellow')
        self.assertEqual(root['background'], 'blue')
        self.assertEqual(root['highlightcolor'], 'yellow')
        root.tk_setPalette(background='yellow', highlightColor='blue')
        self.assertEqual(root['background'], 'yellow')
        self.assertEqual(root['highlightcolor'], 'blue')
        self.assertRaisesRegex(tkinter.TclError,
                '^unknown color name "spam"$',
                root.tk_setPalette, background='spam')
        self.assertRaisesRegex(tkinter.TclError,
                '^must specify a background color$',
                root.tk_setPalette, spam='white')
        self.assertRaisesRegex(tkinter.TclError,
                '^must specify a background color$',
                root.tk_setPalette, highlightColor='blue')

    def test_after(self):
        root = self.root

        def callback(start=0, step=1):
            nonlocal count
            count = start + step

        # Without function, sleeps for ms.
        self.assertIsNone(root.after(1))

        # Set up with callback with no args.
        count = 0
        timer1 = root.after(0, callback)
        self.assertIn(timer1, root.tk.call('after', 'info'))
        (script, _) = root.tk.splitlist(root.tk.call('after', 'info', timer1))
        root.update()  # Process all pending events.
        self.assertEqual(count, 1)
        with self.assertRaises(tkinter.TclError):
            root.tk.call(script)

        # Set up with callback with args.
        count = 0
        timer1 = root.after(0, callback, 42, 11)
        root.update()  # Process all pending events.
        self.assertEqual(count, 53)

        # Cancel before called.
        timer1 = root.after(1000, callback)
        self.assertIn(timer1, root.tk.call('after', 'info'))
        (script, _) = root.tk.splitlist(root.tk.call('after', 'info', timer1))
        root.after_cancel(timer1)  # Cancel this event.
        self.assertEqual(count, 53)
        with self.assertRaises(tkinter.TclError):
            root.tk.call(script)

    def test_after_idle(self):
        root = self.root

        def callback(start=0, step=1):
            nonlocal count
            count = start + step

        # Set up with callback with no args.
        count = 0
        idle1 = root.after_idle(callback)
        self.assertIn(idle1, root.tk.call('after', 'info'))
        (script, _) = root.tk.splitlist(root.tk.call('after', 'info', idle1))
        root.update()  # Process all pending events.
        self.assertEqual(count, 1)
        with self.assertRaises(tkinter.TclError):
            root.tk.call(script)

        # Set up with callback with args.
        count = 0
        idle1 = root.after_idle(callback, 42, 11)
        root.update()  # Process all pending events.
        self.assertEqual(count, 53)

        # Cancel before called.
        idle1 = root.after_idle(callback)
        self.assertIn(idle1, root.tk.call('after', 'info'))
        (script, _) = root.tk.splitlist(root.tk.call('after', 'info', idle1))
        root.after_cancel(idle1)  # Cancel this event.
        self.assertEqual(count, 53)
        with self.assertRaises(tkinter.TclError):
            root.tk.call(script)

    def test_after_cancel(self):
        root = self.root

        def callback():
            nonlocal count
            count += 1

        timer1 = root.after(5000, callback)
        idle1 = root.after_idle(callback)

        # No value for id raises a ValueError.
        with self.assertRaises(ValueError):
            root.after_cancel(None)

        # Cancel timer event.
        count = 0
        (script, _) = root.tk.splitlist(root.tk.call('after', 'info', timer1))
        root.tk.call(script)
        self.assertEqual(count, 1)
        root.after_cancel(timer1)
        with self.assertRaises(tkinter.TclError):
            root.tk.call(script)
        self.assertEqual(count, 1)
        with self.assertRaises(tkinter.TclError):
            root.tk.call('after', 'info', timer1)

        # Cancel same event - nothing happens.
        root.after_cancel(timer1)

        # Cancel idle event.
        count = 0
        (script, _) = root.tk.splitlist(root.tk.call('after', 'info', idle1))
        root.tk.call(script)
        self.assertEqual(count, 1)
        root.after_cancel(idle1)
        with self.assertRaises(tkinter.TclError):
            root.tk.call(script)
        self.assertEqual(count, 1)
        with self.assertRaises(tkinter.TclError):
            root.tk.call('after', 'info', idle1)

    def test_clipboard(self):
        root = self.root
        root.clipboard_clear()
        root.clipboard_append('Ùñî')
        self.assertEqual(root.clipboard_get(), 'Ùñî')
        root.clipboard_append('çōđě')
        self.assertEqual(root.clipboard_get(), 'Ùñîçōđě')
        root.clipboard_clear()
        with self.assertRaises(tkinter.TclError):
            root.clipboard_get()

    def test_clipboard_astral(self):
        root = self.root
        root.clipboard_clear()
        root.clipboard_append('𝔘𝔫𝔦')
        self.assertEqual(root.clipboard_get(), '𝔘𝔫𝔦')
        root.clipboard_append('𝔠𝔬𝔡𝔢')
        self.assertEqual(root.clipboard_get(), '𝔘𝔫𝔦𝔠𝔬𝔡𝔢')
        root.clipboard_clear()
        with self.assertRaises(tkinter.TclError):
            root.clipboard_get()

    def test_mainloop_dispatching(self):
        # reconstruct default root destroyed by AbstractTkTest
        root = tkinter._default_root = self.root
        for obj in (root.tk, root, tkinter):
            self.assertFalse(obj.dispatching())
            root.after(0, lambda:self.assertTrue(obj.dispatching()))

            # guarantees mainloop end after first call to Tcl_DoOneEvent
            root.after(0, root.quit)

            root.mainloop()
            self.assertFalse(obj.dispatching())

    def test_willdispatch(self):
        root = self.root
        self.assertFalse(root.dispatching())
        with self.assertWarns(DeprecationWarning):
            root.tk.willdispatch()
        self.assertTrue(root.dispatching())

    def test_thread_must_wait_for_mainloop(self):
        sentinel = object()
        thread_properly_raises = sentinel
        thread_dispatching_early = sentinel
        thread_dispatching_eventually = sentinel

        def target():
            nonlocal thread_dispatching_early
            nonlocal thread_properly_raises
            nonlocal thread_dispatching_eventually

            try:
                thread_dispatching_early = root.dispatching()
                root.after(0)  # Null op
            except RuntimeError as e:
                if str(e) == "main thread is not in main loop":
                    thread_properly_raises=True
                else:
                    thread_properly_raises=False
                    raise
            else:
                thread_properly_raises=False
                return
            finally:
                # must guarantee that any reason not to run mainloop
                # is flagged in the above try/except/else and will
                # keep the main thread from calling root.mainloop()
                ready_for_mainloop.set()

            # Everything after the event set must go in this try
            # to guarantee that root.quit() is called in finally
            try:
                # self.assertTrue(root.dispatching()) but patient
                for i in range (1000):
                    if root.dispatching():
                        thread_dispatching_eventually = True
                        break
                    time.sleep(0.01)
                else:  # if not break
                    thread_dispatching_eventually = False
            finally:
                root.after(0, root.quit)

        root = self.root

        # remove on eventual WaitForMainloop behavior change
        root.tk.setmainloopwaitattempts(0)

        try:
            ready_for_mainloop = threading.Event()
            thread = threading.Thread(target=target, daemon=True)
            self.assertFalse(root.dispatching())
            thread.start()
            ready_for_mainloop.wait()

            # if these fail we don't want to risk starting mainloop
            self.assertFalse(thread_dispatching_early is sentinel)
            self.assertFalse(thread_dispatching_early)
            self.assertFalse(thread_properly_raises is sentinel)
            self.assertTrue(thread_properly_raises)

            root.mainloop()
            self.assertFalse(root.dispatching())
            thread.join()
        finally:
            # this global *must* be reset
            with self.assertWarns(DeprecationWarning):
                root.tk.setmainloopwaitattempts(10)

        self.assertFalse(thread_dispatching_eventually is sentinel)
        self.assertTrue(thread_dispatching_eventually)



tests_gui = (MiscTest, )

if __name__ == "__main__":
    support.run_unittest(*tests_gui)
