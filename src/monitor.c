#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>

#define MSG_KEY 1234
#define ALERT_PIPE "/tmp/alert_pipe"
#define MAX_AMOUNT 10000

struct transaction {
    long msg_type;
    int account_id;
    double amount;
    char type[10];
};

void analyze_transaction(struct transaction *trans);
void send_alert(int account_id, double amount, const char *type);

int main() {
    int msgid;
    struct transaction trans;

    // Inicializa y abre el mensaje
    if ((msgid = msgget(MSG_KEY, 0666 | IPC_CREAT)) == -1) {
        perror("msgget");
        exit(EXIT_FAILURE);
    }

    // Continua leyendo el mensaje
    while (1) {
        if (msgrcv(msgid, &trans, sizeof(struct transaction) - sizeof(long), 0, 0) == -1) {
            perror("msgrcv");
            exit(EXIT_FAILURE);
        }

        // Analiza las transaccoiones
        analyze_transaction(&trans);
    }

    // Cierra el mensaje
    if (msgctl(msgid, IPC_RMID, NULL) == -1) {
        perror("msgctl");
        exit(EXIT_FAILURE);
    }

    return 0;
}

// Analizar las transacciones
void analyze_transaction(struct transaction *trans) {
    static int consecutive_withdrawals = 0;
    static int last_account_id = -1;

    if (strcmp(trans->type, "withdrawal") == 0) {
        if (trans->amount > MAX_AMOUNT) {
            send_alert(trans->account_id, trans->amount, trans->type);
        }

        if (trans->account_id == last_account_id) {
            consecutive_withdrawals++;
            if (consecutive_withdrawals > 3) {
                send_alert(trans->account_id, trans->amount, "consecutive withdrawals");
            }
        } else {
            consecutive_withdrawals = 1;
            last_account_id = trans->account_id;
        }
    } else if (strcmp(trans->type, "transfer") == 0) {
        // Detecta si hay algo sospecho
    }
}

void send_alert(int account_id, double amount, const char *type) {
    FILE *pipe = fopen(ALERT_PIPE, "w");
    if (pipe == NULL) {
        perror("fopen");
        return;
    }

    fprintf(pipe, "ALERT: Account %d, Amount %.2f, Type %s\n", account_id, amount, type);
    fclose(pipe);
}
