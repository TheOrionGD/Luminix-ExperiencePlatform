#include "transaction.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

static int g_next_txn_id = 1;
static pthread_mutex_t g_txn_id_mutex = PTHREAD_MUTEX_INITIALIZER;

static int generate_txn_id(void) {
    pthread_mutex_lock(&g_txn_id_mutex);
    int id = g_next_txn_id++;
    pthread_mutex_unlock(&g_txn_id_mutex);
    return id;
}

Transaction* transaction_begin(void) {
    Transaction* txn = malloc(sizeof(Transaction));
    if (!txn) return NULL;
    
    txn->id = generate_txn_id();
    txn->state = TXN_ACTIVE;
    txn->operations = NULL;
    txn->op_count = 0;
    
    return txn;
}

int transaction_commit(Transaction* txn) {
    if (!txn || txn->state != TXN_ACTIVE) {
        return -1;
    }
    
    // Clean up allocated operations if any
    if (txn->operations) {
        for (int i = 0; i < txn->op_count; i++) {
            if (txn->operations[i]) {
                free(txn->operations[i]);
            }
        }
        free(txn->operations);
        txn->operations = NULL;
    }
    txn->op_count = 0;
    txn->state = TXN_COMMITTED;
    
    return 0;
}

void transaction_rollback(Transaction* txn) {
    if (!txn || txn->state != TXN_ACTIVE) return;
    
    // Clean up allocated operations if any
    if (txn->operations) {
        for (int i = 0; i < txn->op_count; i++) {
            if (txn->operations[i]) {
                free(txn->operations[i]);
            }
        }
        free(txn->operations);
        txn->operations = NULL;
    }
    txn->op_count = 0;
    txn->state = TXN_ABORTED;
}

void transaction_free(Transaction* txn) {
    if (txn) {
        if (txn->operations) {
            for (int i = 0; i < txn->op_count; i++) {
                if (txn->operations[i]) {
                    free(txn->operations[i]);
                }
            }
            free(txn->operations);
        }
        free(txn);
    }
}
