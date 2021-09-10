#ifndef PYSQLITE_BLOB_H
#define PYSQLITE_BLOB_H
#include "Python.h"
#include "sqlite3.h"
#include "connection.h"

typedef struct {
    PyObject_HEAD
    pysqlite_Connection *connection;
    sqlite3_blob *blob;
    int offset;
    int length;

    PyObject *in_weakreflist;
} pysqlite_Blob;

extern PyTypeObject pysqlite_BlobType;

PyObject *pysqlite_blob_close(pysqlite_Blob *self);

int pysqlite_blob_setup_types(void);
void pysqlite_close_all_blobs(pysqlite_Connection *self);

#endif
