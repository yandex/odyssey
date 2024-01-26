
/*
 * Odyssey.
 *
 * Scalable PostgreSQL connection pooler.
 */

#include <odyssey.h>
#include <sys/poll.h>
#include <sys/un.h>
#include <sys/socket.h>

/*CONNECTION CALLBACK TYPES*/
#define MDB_IAMPROXY_CONN_ERROR -1
#define MDB_IAMPROXY_CONN_TIMEOUT -1
#define MDB_IAMPROXY_CONN_ACCEPTED 0
#define MDB_IAMPROXY_CONN_DENIED -1

#define MDB_IAMPROXY_RES_ERROR -1
#define MDB_IAMPROXY_RES_OK 0

/*AUTHENTICATION TIMEOUT LIMIT*/
#define MDB_IAMPROXY_BYTE_SIZE 8
#define MDB_IAMPROXY_DEFAULT_HEADER_SIZE 8
#define MDB_IAMPROXY_DEFAULT_CNT_CONNECTIONS 1

#define MDB_IAMPROXY_DEFAULT_CONNECTION_TIMEOUT 1000
#define MDB_IAMPROXY_DEFAULT_RECEIVING_TIMEOUT 1000

/*PAM SOCKET FILE*/
#define MDB_IAMPROXY_DEFAULT_SOCKET_FILE                                                    \
    "/var/run/iam-auth-proxy/iam-auth-proxy.sock" // PAM SOCKET FILE place

int mdb_iamproxy_recv_from_socket(int socket_fd, char *msg_body) {
    /*GET COMMON MSG INFO AND ALLOCATE RESOURCES*/
    int64_t recv_result = MDB_IAMPROXY_CONN_ACCEPTED;
    uint64_t body_size = 0;
    unsigned char header_byte;

    /*RECIEVE HEADER*/
    for (int i = 0; i < MDB_IAMPROXY_BYTE_SIZE; ++i) {
        if (recv(socket_fd, &header_byte, sizeof(header_byte), 0) < 0) { // error during recieve msg header byte
            recv_result = MDB_IAMPROXY_CONN_ERROR;
            goto free_start;
        }
        body_size = (body_size | (((unsigned)header_byte) << (MDB_IAMPROXY_BYTE_SIZE * i)));
    }

    /*RECIEVE BODY*/
    if (recv(socket_fd, msg_body, body_size, 0) < 0) { // error during recieing nsg body
        recv_result = MDB_IAMPROXY_CONN_ERROR;
        goto free_end;
    }

free_start:
free_end:
    return recv_result;
}

int mdb_iamproxy_send_to_socket(int socket_fd, const char *send_msg) {
    /*GET COMMON MSG INFO AND ALLOCATE BUFFER*/
    int32_t send_result = MDB_IAMPROXY_RES_OK;
    uint64_t body_size = strlen(send_msg) + 1; // stores size of message (add one byte for 'c\0')
    uint64_t current_body_size = body_size;
    uint64_t msg_size = sizeof(body_size) + body_size;
    char *msg = (char *)calloc(msg_size, sizeof(*msg)); // allocate memory for msg buffer
    if (msg == NULL) { // error during allocating memory for msg buffer
        send_result = MDB_IAMPROXY_RES_ERROR;
        goto free_end;
    }

    /*COPY ALL DATA TO BUFFER FOR SENDING*/
    for (int i = 0; i < MDB_IAMPROXY_DEFAULT_HEADER_SIZE; ++i) { // coping header to msg buffer
        msg[i] = (current_body_size & 0xFF);
        current_body_size >>= MDB_IAMPROXY_BYTE_SIZE;
    }
    memcpy(msg + sizeof(body_size), send_msg, body_size); // coping body to msg buffer

    /*SEND TO SOCKET*/
    if (send(socket_fd, msg, msg_size, 0) < 0) { // error during sending data
        send_result = MDB_IAMPROXY_RES_ERROR;
        goto free_start;
    }

free_start:
    free(msg);
free_end:
    return send_result;
}

int mdb_iamproxy_authenticate_user(const char *username, const char *token, od_instance_t *instance, od_client_t *client) {
    char auth_status = 0; // auth_status stores one byte if it's 0 => not authenticated
    char external_user[512]; // store subject_id of authenticated client
    int32_t authentication_result = MDB_IAMPROXY_CONN_DENIED; // stores authenticate status for user (default value: CONN_DENIED)
    int32_t correct_sending = MDB_IAMPROXY_CONN_ACCEPTED; // stores stutus of sending data to iam-auth-proxy
    int32_t correct_recieving = MDB_IAMPROXY_CONN_ACCEPTED; // store status of recieving data from iam-auth-proxy
    int64_t socket_fd; // stores file descripotor for DEFAULT_SOCKET_FILE
    int64_t poll_result = 1; // stores return value of poll() function

    /*SOCKET SETUP*/
    struct sockaddr_un exchange_socket; // socket for interprocceses connection
    memset(&exchange_socket, 0, sizeof(exchange_socket));
    exchange_socket.sun_family = AF_UNIX;
    strcpy(exchange_socket.sun_path, MDB_IAMPROXY_DEFAULT_SOCKET_FILE);

    /*GET SOCKET FILE DESCRIPTOR*/
    socket_fd = socket(AF_UNIX, SOCK_STREAM, 0); // get socket file descriptor
    if (socket_fd < 0) { // error during getting socket file descriptor
        authentication_result = MDB_IAMPROXY_CONN_ERROR;
        goto free_end;
    }

    /*SET SOCKET FLAGS AND WRITE SOCKET_FD to fds*/
    fcntl(socket_fd, F_SETFL, O_NONBLOCK); // set non block flag for connection
    struct pollfd fds;  // stores info about socket_fd and it's (socket_fd) status
    fds.fd = socket_fd; // set socket_value
    fds.events = POLLOUT; // waiting for write

    /*CONNECT TO SOCKET*/
    connect(socket_fd, (struct sockaddr *)&exchange_socket, sizeof(exchange_socket));

    /*WAIT FOR CONNECTION*/
    poll_result = poll(&fds, MDB_IAMPROXY_DEFAULT_CNT_CONNECTIONS, MDB_IAMPROXY_DEFAULT_CONNECTION_TIMEOUT);
    if (poll_result == -1) { // error during connecting to socket
        authentication_result = MDB_IAMPROXY_CONN_ERROR;
        goto free_start;
    } else if (poll_result == 0) { // reach timeout shile waiting for socket
        authentication_result = MDB_IAMPROXY_CONN_TIMEOUT;
        goto free_start;
    }

    /*COMMUNICATE WITH SOCKET*/
    correct_sending = mdb_iamproxy_send_to_socket(socket_fd, username); // send USERNAME to socket
    if (correct_sending != MDB_IAMPROXY_RES_OK) { // error during sending data to socket
        authentication_result = correct_sending;
        goto free_start;
    }
    correct_sending = mdb_iamproxy_send_to_socket(socket_fd, token); // send TOKEN to socket
    if (correct_sending != MDB_IAMPROXY_RES_OK) { // error during sending data to socket
        authentication_result = correct_sending;
        goto free_start;
    }

    /*WAIT FOR IAM-PROXY RESPONSE*/
    fds.events = POLLIN;
    poll_result = poll(&fds, MDB_IAMPROXY_DEFAULT_CNT_CONNECTIONS, MDB_IAMPROXY_DEFAULT_RECEIVING_TIMEOUT);
    if (poll_result == -1) { // error during waiting for reading from socket
        authentication_result = MDB_IAMPROXY_CONN_ERROR;
        goto free_start;
    } else if (poll_result == 0) { // reach timeout while waiting for socket
        authentication_result = MDB_IAMPROXY_CONN_TIMEOUT;
        goto free_start;
    }

    /*COMMUNUCATE WITH SOCKET*/
    correct_recieving = mdb_iamproxy_recv_from_socket(socket_fd, &auth_status); // recieve auth_status from socket
    if (correct_recieving != MDB_IAMPROXY_CONN_ACCEPTED) { // recieving is not completed successfully
        authentication_result = correct_recieving;
        goto free_start;
    }

    if ((unsigned)auth_status) {
        authentication_result = MDB_IAMPROXY_CONN_ACCEPTED;
    } else {
        authentication_result = MDB_IAMPROXY_CONN_DENIED;
    }

    correct_recieving = mdb_iamproxy_recv_from_socket(socket_fd, external_user); // recieve subject_id from socket
    if (correct_recieving != MDB_IAMPROXY_CONN_ACCEPTED) { // recieveing is not completed successfully
        authentication_result = correct_recieving;
        goto free_start;
    }

    od_log(&instance->logger, "auth", client, NULL,
            "user '%s.%s' was authenticated with subject_id: %s",
            client->startup.database.value, client->startup.user.value, external_user);

    /*FREE RESOURCES*/
free_start:
    close(socket_fd);
free_end:
    /*RETURN RESULT*/
    return authentication_result;
}
