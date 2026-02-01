
/**
 * Transaction Header File
 */

#ifndef TRANSACTION_H
#define TRANSACTION_H

typedef enum {
    TXN_ACTIVE,
    TXN_COMMITTED,
    TXN_ABORTED
} TransactionState;

typedef struct {
    int id;
    TransactionState state;
    void** operations;
    int op_count;
} Transaction;

Transaction* transaction_begin(void);
int transaction_commit(Transaction* txn);
void transaction_rollback(Transaction* txn);
void transaction_free(Transaction* txn);

#endif
