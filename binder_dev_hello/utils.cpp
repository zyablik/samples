#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sstream>
#include <sys/ioctl.h>
#include "utils.h"

// reply buffer usually contains several commands
// the main command is BR_REPLY, if buffer doesn't
// have it than we need to do one more ioctl(BINDER_WRITE_READ)
binder_transaction_data * transact(int binder_dev_fd, struct binder_write_read& bwr) {
    binder_transaction_data * reply = nullptr;
    do {
        if(ioctl(binder_dev_fd, BINDER_WRITE_READ, &bwr) == -1)
            pexit("error while ioctl(BINDER_WRITE_READ, PING_TRANSACTION)");

        uintptr_t ptr = bwr.read_buffer;
        uintptr_t end = ptr + (uintptr_t) bwr.read_consumed;
        printf("ptr = %lu end = %lu\n", ptr, end);
        while (ptr < end) {
            binder_driver_return_protocol cmd = (binder_driver_return_protocol)*(uint32_t *) ptr;
            ptr += sizeof(uint32_t);
            printf("binder reply received: %s\n", binder_driver_reply_str(cmd).c_str());

            switch(cmd) {
                case BR_NOOP:
                case BR_TRANSACTION_COMPLETE:
                    break;
                case BR_INCREFS:
                case BR_ACQUIRE:
                case BR_RELEASE:
                case BR_DECREFS:
                    printf("%p, %p\n", (void *)ptr, (void *)(ptr + sizeof(void *)));
                    ptr += sizeof(struct binder_ptr_cookie);
                    break;
                case BR_TRANSACTION: {
                    struct binder_transaction_data * txn = (struct binder_transaction_data *) ptr;
                    if ((end - ptr) < sizeof(*txn)) {
                        printf("parse: txn too small!\n");
                        return nullptr;
                    }

                    binder_dump_txn(txn);

                    typedef void (*fptr)(struct binder_transaction_data *);
                    reinterpret_cast<fptr>(txn->target.ptr)(txn);
                    reply = txn;

    //                     res = func(bs, txn, &msg, &reply);
    //                     if (txn->flags & TF_ONE_WAY) {
    //                         binder_free_buffer(bs, txn->data.ptr.buffer);
    //                     } else {
    //                         binder_send_reply(bs, &reply, txn->data.ptr.buffer, res);
    //                     }
                    ptr += sizeof(*txn);
                    break;
                }
                case BR_REPLY: {
                    struct binder_transaction_data * txn = (struct binder_transaction_data *) ptr;
                    if ((end - ptr) < sizeof(*txn)) {
                        printf("parse: reply too small!\n");
                        return nullptr;
                    }
                    ptr += sizeof(*txn);

                    binder_dump_txn(txn);

                    reply = txn;
                    break;
                }
                case BR_DEAD_BINDER: {
    //                    struct binder_death * death = (struct binder_death *)(uintptr_t) *(binder_uintptr_t *)ptr;
                    ptr += sizeof(binder_uintptr_t);
    //                    death->func(bs, death->ptr);
                    break;
                }
                case BR_FAILED_REPLY:
                case BR_DEAD_REPLY:
                    return nullptr;
                    break;
                default:
                    printf("parse: unknown BR_ cmd %d\n", cmd);
            }
        }
        bwr.write_size = 0;
        bwr.write_consumed = 0;
        bwr.write_buffer = 0;
    } while(!reply);

    return reply;
}

void free_buffer(int binder_dev_fd, uintptr_t txn_data_buffer) {
    struct {
        uint32_t cmd;
        uintptr_t buffer;
    } __attribute__((packed)) free_buffer_cmd_data;

    free_buffer_cmd_data.cmd = BC_FREE_BUFFER;
    free_buffer_cmd_data.buffer = txn_data_buffer;

    struct binder_write_read bwr = {};
    bwr.write_buffer = (uintptr_t) &free_buffer_cmd_data;
    bwr.write_size = sizeof(free_buffer_cmd_data);
    if(ioctl(binder_dev_fd, BINDER_WRITE_READ, &bwr) == -1)
        pexit("error while ioctl(BINDER_WRITE_READ, BC_FREE_BUFFER)");
}

void binder_acquire(int binder_dev_fd, uint32_t handle) {
    uint32_t acquire_cmd_data[2] = { BC_ACQUIRE, handle };
    struct binder_write_read bwr = {};
    bwr.write_buffer = (uintptr_t) &acquire_cmd_data;
    bwr.write_size = sizeof(acquire_cmd_data);
    if(ioctl(binder_dev_fd, BINDER_WRITE_READ, &bwr) == -1)
        pexit("error while ioctl(BINDER_WRITE_READ, BC_FREE_BUFFER)");
}

void pexit(const char * format, ...) {
    va_list arglist;
    va_start(arglist, format);
    vprintf(format, arglist);
    va_end(arglist);
    printf(": errno = %d: %s. exit(1)\n", errno, strerror(errno));
    exit(1);
}

std::string binder_driver_command_str(binder_driver_command_protocol value) {
    switch(value) {
        case BC_TRANSACTION: return "BC_TRANSACTION";
        case BC_REPLY: return "BC_REPLY";
        case BC_ACQUIRE_RESULT: return "BC_ACQUIRE_RESULT";
        case BC_FREE_BUFFER: return "BC_FREE_BUFFER";
        case BC_INCREFS: return "BC_INCREFS";
        case BC_ACQUIRE: return "BC_ACQUIRE";
        case BC_RELEASE: return "BC_RELEASE";
        case BC_DECREFS: return "BC_DECREFS";
        case BC_INCREFS_DONE: return "BC_INCREFS_DONE";
        case BC_ACQUIRE_DONE: return "BC_ACQUIRE_DONE";
        case BC_ATTEMPT_ACQUIRE: return "BC_ATTEMPT_ACQUIRE";
        case BC_REGISTER_LOOPER: return "BC_REGISTER_LOOPER";
        case BC_ENTER_LOOPER: return "BC_ENTER_LOOPER";
        case BC_EXIT_LOOPER: return "BC_EXIT_LOOPER";
        case BC_REQUEST_DEATH_NOTIFICATION: return "BC_REQUEST_DEATH_NOTIFICATION";
        case BC_CLEAR_DEATH_NOTIFICATION: return "BC_CLEAR_DEATH_NOTIFICATION";
        case BC_DEAD_BINDER_DONE: return "BC_DEAD_BINDER_DONE";
        default: return (std::stringstream() << "UNKNOWN_BC_COMMAND: " << value).str();
    }
}

std::string binder_driver_reply_str(binder_driver_return_protocol value) {
    switch(value) {
        case BR_ERROR:                         return "BR_ERROR";
        case BR_OK:                            return "BR_OK";
        case BR_TRANSACTION:                   return "BR_TRANSACTION";
        case BR_REPLY:                         return "BR_REPLY";
        case BR_ACQUIRE_RESULT:                return "BR_ACQUIRE_RESULT";
        case BR_DEAD_REPLY:                    return "BR_DEAD_REPLY";
        case BR_TRANSACTION_COMPLETE:          return "BR_TRANSACTION_COMPLETE";
        case BR_INCREFS:                       return "BR_INCREFS";
        case BR_ACQUIRE:                       return "BR_ACQUIRE";
        case BR_RELEASE:                       return "BR_RELEASE";
        case BR_DECREFS:                       return "BR_DECREFS";
        case BR_ATTEMPT_ACQUIRE:               return "BR_ATTEMPT_ACQUIRE";
        case BR_NOOP:                          return "BR_NOOP";
        case BR_SPAWN_LOOPER:                  return "BR_SPAWN_LOOPER";
        case BR_FINISHED:                      return "BR_FINISHED";
        case BR_DEAD_BINDER:                   return "BR_DEAD_BINDER";
        case BR_CLEAR_DEATH_NOTIFICATION_DONE: return "BR_CLEAR_DEATH_NOTIFICATION_DONE";
        case BR_FAILED_REPLY:                  return "BR_FAILED_REPLY";
        default:                               return (std::stringstream() << "UNKNOWN_BR_REPLY: " << value).str();
    }
}

std::string transaction_flags_str(uint32_t flags) {
    std::string str;
    if(flags & TF_ONE_WAY)     str += "TF_ONE_WAY|";
    if(flags & TF_ROOT_OBJECT) str += "TF_ROOT_OBJECT|";
    if(flags & TF_STATUS_CODE) str += "TF_STATUS_CODE|";
    if(flags & TF_ACCEPT_FDS)  str += "TF_ACCEPT_FDS|";
    return str.substr(0, str.size() - 1);
}

std::string flat_binder_object_flags_str(uint32_t flags) {
    std::string str;
    if(flags & FLAT_BINDER_FLAG_PRIORITY_MASK)     str += "FLAT_BINDER_FLAG_PRIORITY_MASK|";
    if(flags & FLAT_BINDER_FLAG_ACCEPTS_FDS) str += "FLAT_BINDER_FLAG_ACCEPTS_FDS|";
    return str.substr(0, str.size() - 1);
}

void hexdump(void *_data, size_t len) {
    uint8_t * data = (uint8_t *)_data;
    size_t count;

    for (count = 0; count < len; count++) {
        if ((count & 15) == 0)
            printf("%04zu:", count);

        printf(" %02x %c", *data, (*data < 32) || (*data > 126) ? '.' : *data);
        data++;
        if ((count & 15) == 15)
            printf("\n");
    }
    if ((count & 15) != 0)
        printf("\n");
}

void binder_dump_txn(struct binder_transaction_data * txn) {
    struct flat_binder_object * obj;
    binder_size_t * offs = (binder_size_t *)(uintptr_t)txn->data.ptr.offsets;
    size_t count = txn->offsets_size / sizeof(binder_size_t);

    printf("  target %016" PRIx64 "  cookie %016" PRIx64 "  code %08x  flags %08x (%s)\n",
            (uint64_t)txn->target.ptr, (uint64_t)txn->cookie, txn->code, txn->flags, transaction_flags_str(txn->flags).c_str());
    printf("  pid %8d  uid %8d  data %" PRIu64 "  offs %" PRIu64 "\n",
            txn->sender_pid, txn->sender_euid, (uint64_t)txn->data_size, (uint64_t)txn->offsets_size);
    hexdump((void *)(uintptr_t)txn->data.ptr.buffer, txn->data_size);
    while (count--) {
        obj = (struct flat_binder_object *) (((char*)(uintptr_t)txn->data.ptr.buffer) + *offs++);
        printf("  - type %08x (%c%c%c)  flags %08x (%s) binder(handle) %016" PRIx64 "  cookie %016" PRIx64 "\n",
                obj->type, ((char *)&obj->type)[3], ((char *)&obj->type)[2], ((char *)&obj->type)[1],
               obj->flags, flat_binder_object_flags_str(obj->flags).c_str(), (uint64_t)obj->binder, (uint64_t)obj->cookie);
    }
}
