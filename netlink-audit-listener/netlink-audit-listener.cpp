#include <errno.h>
#include <memory.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sstream>
#include <vector>

#include <linux/audit.h>
#include <sys/socket.h>


#include <linux/netlink.h>

/* Flags for ACK message */
#if !defined(NLM_F_CAPPED)
#define NLM_F_CAPPED    0x100   /* request was capped */
#endif

#if !defined(NLM_F_ACK_TLVS)
#define NLM_F_ACK_TLVS  0x200   /* extended ACK TVLs were included */
#endif

bool verbose = false;

struct nlmsg {
    struct nlmsghdr hdr;
    std::vector<uint8_t> payload;
};

const char * audit_type_to_str(uint16_t type);
std::string audit_flags_to_str(uint16_t flags);
std::string audit_msg_to_str(const nlmsg& msg, bool hdr_only = false);

nlmsg audit_recv(int fd);
uint32_t audit_send(int fd, int type, const void * data, unsigned int size, bool request_ack);

int main(int argc, char *argv[]) {
    // ------------------
    // 1. process cmdline
    if(argc == 2) {
        if(argv[1] == std::string("-v")) {
            verbose = true;
        } else {
            printf("usage: %s [-v]\n", program_invocation_short_name);
            exit(EXIT_FAILURE);
        }
    }

    // ------------------
    // 2. init netlink audit socket

    int audit_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_AUDIT);
    if (audit_fd < 0) {
        printf("[%s] socket(..., NETLINK_AUDIT) failed: %d: %s. exit()\n", program_invocation_short_name, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

#if defined(NETLINK_CAP_ACK)
    int one = 1;
    setsockopt(audit_fd, SOL_NETLINK, NETLINK_CAP_ACK, &one, sizeof(one)); // do not attach msg data to ack
#endif

    printf("[%s] socket(NETLINK_AUDIT) = %d\n", program_invocation_short_name, audit_fd);

    // ------------------
    // 3. get current audit status

    printf("[%s] get current status:\n", program_invocation_short_name);
    uint32_t audit_get_seqno = audit_send(audit_fd, AUDIT_GET, NULL, 0, false);
    while(true) {
        nlmsg reply = audit_recv(audit_fd);
        if (reply.hdr.nlmsg_seq == audit_get_seqno) {
            printf("  %s\n", audit_msg_to_str(reply).c_str());
            if (reply.hdr.nlmsg_type == AUDIT_GET) {
                struct audit_status * status = (struct audit_status *) reply.payload.data();
                if(status->pid != 0) {
                    printf("[%s] ERROR: netlink audit socket is already BUSY by pid = %d. only one consumer could be registered. exit().\n", program_invocation_short_name, status->pid);
                    exit(EXIT_FAILURE);
                }
            }
            break;
        } else {
            printf("skip unexpected reply for AUDIT_GET: type: %s seq: %u\n", audit_type_to_str(reply.hdr.nlmsg_type), reply.hdr.nlmsg_seq);
        }
    };

    // ------------------
    // 4. force set audit status: enable it, assign self as audit consumer and disable rate limit

    printf("[%s] force status = enabled, rate limit = 0, pid = %d\n", program_invocation_short_name, getpid());
    struct audit_status s = { 0 };
    s.mask = AUDIT_STATUS_RATE_LIMIT | AUDIT_STATUS_PID | AUDIT_STATUS_ENABLED | AUDIT_STATUS_FAILURE;
    s.rate_limit = 0;
    s.pid = getpid();
    s.enabled = 1;
    s.failure = AUDIT_FAIL_PRINTK;

    uint32_t audit_set_seqno = audit_send(audit_fd, AUDIT_SET, &s, sizeof(s), true);

    while(true) {
        nlmsg reply = audit_recv(audit_fd);
        if (reply.hdr.nlmsg_seq == audit_set_seqno) {
            if(reply.hdr.nlmsg_type == NLMSG_ERROR) {
                struct nlmsgerr * nle = (struct nlmsgerr *) reply.payload.data();
                if(nle->error != 0) {
                    if(nle->error == -EPERM) {
                        printf("[%s] AUDIT_SET failed with EPERM error. probably sudo/root required. exit().\n", program_invocation_short_name);
                    } else {
                        printf("[%s] AUDIT_SET error occured: %s. exit(EXIT_FAILURE).\n", program_invocation_short_name, audit_msg_to_str(reply).c_str());
                    }
                    exit(EXIT_FAILURE);
                }
            }
            break;
        } else {
            printf("skip unexpected reply for AUDIT_SET: type: %s seq: %u\n", audit_type_to_str(reply.hdr.nlmsg_type), reply.hdr.nlmsg_seq);
        }
    };

    // ------------------
    // 4.1 get status again
    printf("[%s] get status again:\n", program_invocation_short_name);
    audit_send(audit_fd, AUDIT_GET, NULL, 0, false);

    // ------------------
    // 5. receive audit messages forever

    printf("[%s] doing recv loop...\n\n", program_invocation_short_name);
    while(true) {
        nlmsg reply = audit_recv(audit_fd);
        if(reply.hdr.nlmsg_type == NLMSG_ERROR) {
            struct nlmsgerr * nle = (struct nlmsgerr *) reply.payload.data();
            if(nle->error != 0) {
                printf("error occured: %s. exit(EXIT_FAILURE).\n", audit_msg_to_str(reply).c_str());
                exit(EXIT_FAILURE);
            }

        } else {
            printf("%s\n\n", audit_msg_to_str(reply).c_str());
        }
    };

    return 0;
}

// return nlmsg_seq
uint32_t audit_send(int fd, int type, const void * data, unsigned int size, bool request_ack) {
    static int sequence = 12345;

    uint8_t nl_msg[NLMSG_SPACE(size)] = { 0 };
    struct nlmsghdr * nlh = (struct nlmsghdr *) nl_msg;

    nlh->nlmsg_len = sizeof(nl_msg);
    nlh->nlmsg_type = type;
    nlh->nlmsg_flags = NLM_F_REQUEST;
    if(request_ack)
        nlh->nlmsg_flags |= NLM_F_ACK;

    nlh->nlmsg_seq = ++sequence;
    nlh->nlmsg_pid = getpid();

    if (size && data)
        memcpy(NLMSG_DATA(nlh), data, size);

    struct sockaddr_nl addr = {
        .nl_family = AF_NETLINK,
        .nl_pid = 0,
        .nl_groups = 0
    };

    int ret = sendto(fd, nlh, nlh->nlmsg_len, 0, (struct sockaddr *) &addr, sizeof(addr));

    std::stringstream msg;
    nlmsg to_print = {
        .hdr = *nlh,
        .payload = { (uint8_t *) data, (uint8_t *) data + size}
    };

    msg << "audit_send(): sendto(): audit_fd: " << fd << ", msg: " << audit_msg_to_str(to_print);

    if(ret != nlh->nlmsg_len) {
        if(ret == -1)
            msg << " failed: errno = " << errno << ": " << strerror(errno) << ". exit().";
        else
            msg << " failed: only " << ret << " bytes of " << nlh->nlmsg_len << " were sent. errno = "
                << errno << " " << strerror(errno) << ". exit()";
        printf("%s\n", msg.str().c_str());
        exit(EXIT_FAILURE);
    } else {
        if(verbose)
            printf("%s, ret: %d. return seq = %u\n", msg.str().c_str(), ret, nlh->nlmsg_seq);
    }

    return nlh->nlmsg_seq;
}

nlmsg audit_recv(int fd) {
    struct sockaddr_nl nladdr;
    socklen_t nladdrlen = sizeof(nladdr);

    uint8_t nl_msg_reply[1024] = { 0 };
    struct nlmsghdr * nlh = (struct nlmsghdr *) nl_msg_reply;

    int ret = recvfrom(fd, nl_msg_reply, sizeof(nl_msg_reply), 0, (struct sockaddr *) &nladdr, &nladdrlen);

    if (ret == -1) {
        printf("audit_recv(): recvfrom() failed: fd = %d errno = %d: %s. exit()\n", fd, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (ret < NLMSG_HDRLEN) {
        printf("audit_recv(): recvfrom() failed: recieved %d bytes: less than NLMSG_HDRLEN (%d). errno = %d: %s. exit()\n",
            ret, NLMSG_HDRLEN, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (ret != nlh->nlmsg_len
        && ret != (nlh->nlmsg_len + 16)) // broadcasted audit mesages have extra 16 bytes after nlmsg. looks like somebody
                                         // thinks that nlmsg_len is not including header [while it actually should]
    {
        printf("audit_recv(): recvfrom() failed: recieved %d bytes while expected nlh->nlmsg_len: %d. errno = %d: %s. exit()\n",
            ret, nlh->nlmsg_len, errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    nlmsg result = {
        .hdr = *nlh,
        .payload = std::vector<uint8_t>((uint8_t *) NLMSG_DATA(nlh), (uint8_t *) nlh + /* nlh->nlmsg_len */ ret)
    };

    if(verbose)
        printf("audit_recv(): recvfrom() ret = %d. msg: %s\n", ret, audit_msg_to_str(result).c_str());

    return std::move(result);
}

std::string audit_msg_to_str(const nlmsg& msg, bool hdr_only) {
    std::stringstream s;
    s << "{ type: " << msg.hdr.nlmsg_type << " [" << audit_type_to_str(msg.hdr.nlmsg_type) << "]"
      << ", len: " << msg.hdr.nlmsg_len
      << ", flags: 0x" << std::hex << msg.hdr.nlmsg_flags << " " << audit_flags_to_str(msg.hdr.nlmsg_flags)
      << ", seq: " << std::dec << msg.hdr.nlmsg_seq << ", " << " pid: " << msg.hdr.nlmsg_pid
      << " }";

    if(!hdr_only && msg.hdr.nlmsg_len > NLMSG_HDRLEN) {
        s << ", payload: \n    ";
        switch(msg.hdr.nlmsg_type) {
            case NLMSG_ERROR: {
                struct nlmsgerr * nle = (struct nlmsgerr *) msg.payload.data();
                nlmsg acked_msg = {
                  .hdr = nle->msg,
                  .payload = {}
                };
                s << "{ error: " << nle->error << " [" << strerror(-nle->error) << "] " << audit_msg_to_str(acked_msg, true) <<  "}";
                break;
            }

            case AUDIT_GET:
            case AUDIT_SET: {
                struct audit_status * status = (struct audit_status *) msg.payload.data();
                s << "{ enabled: " << status->enabled << ", failure: " << status->failure << ", pid: " << status->pid
                  << ", rate_limit: " << status->rate_limit << ", backlog_limit: " << status->backlog_limit
                  << ", lost: " << status->lost << ", backlog: " << status->backlog << " }";
                break;
            }

            default:
                s << std::string(msg.payload.data(), msg.payload.data() + msg.payload.size());
        }
    }

    return s.str();
}

std::string audit_flags_to_str(uint16_t flags) {
    static_assert(std::is_same<decltype(nlmsghdr::nlmsg_flags), decltype(flags)>::value);
    std::stringstream s;
    uint16_t used = 0;
    if(flags & NLM_F_REQUEST) {
        used |= NLM_F_REQUEST;
        s << " NLM_F_REQUEST |";
    }

    if(flags & NLM_F_MULTI) {
        used |= NLM_F_MULTI;
        s << " NLM_F_MULTI |";
    }

    if(flags & NLM_F_ACK) {
        used |= NLM_F_ACK;
        s << " NLM_F_ACK |";
    }

    if(flags & NLM_F_ECHO) {
        used |= NLM_F_ECHO;
        s << " NLM_F_ECHO |";
    }

    bool is_ack = !(flags & NLM_F_REQUEST); // consider any that is not request as ack
    if(is_ack) {
        if(flags & NLM_F_CAPPED) {
            used |= NLM_F_CAPPED;
            s << " NLM_F_CAPPED |";
        }

        if(flags & NLM_F_ACK_TLVS) {
            used |= NLM_F_ACK_TLVS;
            s << " NLM_F_ACK_TLVS |";
        }
    }

    if(flags != used)
        s << " !!! unknown flags = 0x" << std::hex << (flags & ~used) << "|";

    std::string ret = s.str();
    if(ret.empty())
        return "[]";
    else
        ret.pop_back();
        return "[" + ret + "]";
}

const char * audit_type_to_str(uint16_t type) {
  static_assert(std::is_same<decltype(nlmsghdr::nlmsg_type), decltype(type)>::value);
  switch(type) {
    case NLMSG_NOOP: return "NLMSG_NOOP";
    case NLMSG_ERROR: return "NLMSG_ERROR";
    case NLMSG_DONE: return "NLMSG_DONE";
    case NLMSG_OVERRUN: return "NLMSG_OVERRUN";

    case AUDIT_GET: return "AUDIT_GET";
    case AUDIT_SET: return "AUDIT_SET";
    case AUDIT_LIST: return "AUDIT_LIST";
    case AUDIT_ADD: return "AUDIT_ADD";
    case AUDIT_DEL: return "AUDIT_DEL";
    case AUDIT_USER: return "AUDIT_USER";
    case AUDIT_LOGIN: return "AUDIT_LOGIN";
    case AUDIT_WATCH_INS: return "AUDIT_WATCH_INS";
    case AUDIT_WATCH_REM: return "AUDIT_WATCH_REM";
    case AUDIT_WATCH_LIST: return "AUDIT_WATCH_LIST";
    case AUDIT_SIGNAL_INFO: return "AUDIT_SIGNAL_INFO";
    case AUDIT_ADD_RULE: return "AUDIT_ADD_RULE";
    case AUDIT_DEL_RULE: return "AUDIT_DEL_RULE";
    case AUDIT_LIST_RULES: return "AUDIT_LIST_RULES";
    case AUDIT_TRIM: return "AUDIT_TRIM";
    case AUDIT_MAKE_EQUIV: return "AUDIT_MAKE_EQUIV";
    case AUDIT_TTY_GET: return "AUDIT_TTY_GET";
    case AUDIT_TTY_SET: return "AUDIT_TTY_SET";
    case AUDIT_SET_FEATURE: return "AUDIT_SET_FEATURE";
    case AUDIT_GET_FEATURE: return "AUDIT_GET_FEATURE";

    case AUDIT_USER_AVC: return "AUDIT_USER_AVC";
    case AUDIT_USER_TTY: return "AUDIT_USER_TTY";

    case AUDIT_DAEMON_START: return "AUDIT_DAEMON_START";
    case AUDIT_DAEMON_END: return "AUDIT_DAEMON_END";
    case AUDIT_DAEMON_ABORT: return "AUDIT_DAEMON_ABORT";
    case AUDIT_DAEMON_CONFIG: return "AUDIT_DAEMON_CONFIG";

    case AUDIT_SYSCALL: return "AUDIT_SYSCALL";

    case AUDIT_PATH: return "AUDIT_PATH";
    case AUDIT_IPC: return "AUDIT_IPC";
    case AUDIT_SOCKETCALL: return "AUDIT_SOCKETCALL";
    case AUDIT_CONFIG_CHANGE: return "AUDIT_CONFIG_CHANGE";
    case AUDIT_SOCKADDR: return "AUDIT_SOCKADDR";
    case AUDIT_CWD: return "AUDIT_CWD";
    case AUDIT_EXECVE: return "AUDIT_EXECVE";
    case AUDIT_IPC_SET_PERM: return "AUDIT_IPC_SET_PERM";
    case AUDIT_MQ_OPEN: return "AUDIT_MQ_OPEN";
    case AUDIT_MQ_SENDRECV: return "AUDIT_MQ_SENDRECV";
    case AUDIT_MQ_NOTIFY: return "AUDIT_MQ_NOTIFY";
    case AUDIT_MQ_GETSETATTR: return "AUDIT_MQ_GETSETATTR";
    case AUDIT_KERNEL_OTHER: return "AUDIT_KERNEL_OTHER";
    case AUDIT_FD_PAIR: return "AUDIT_FD_PAIR";
    case AUDIT_OBJ_PID: return "AUDIT_OBJ_PID";
    case AUDIT_TTY: return "AUDIT_TTY";
    case AUDIT_EOE: return "AUDIT_EOE";
    case AUDIT_BPRM_FCAPS: return "AUDIT_BPRM_FCAPS";
    case AUDIT_CAPSET: return "AUDIT_CAPSET";
    case AUDIT_MMAP: return "AUDIT_MMAP";
    case AUDIT_NETFILTER_PKT: return "AUDIT_NETFILTER_PKT";
    case AUDIT_NETFILTER_CFG: return "AUDIT_NETFILTER_CFG";
    case AUDIT_SECCOMP: return "AUDIT_SECCOMP";
    case AUDIT_PROCTITLE: return "AUDIT_PROCTITLE";
    case AUDIT_FEATURE_CHANGE: return "AUDIT_FEATURE_CHANGE";

    case AUDIT_AVC: return "AUDIT_AVC";
    case AUDIT_SELINUX_ERR: return "AUDIT_SELINUX_ERR";
    case AUDIT_AVC_PATH: return "AUDIT_AVC_PATH";
    case AUDIT_MAC_POLICY_LOAD: return "AUDIT_MAC_POLICY_LOAD";
    case AUDIT_MAC_STATUS: return "AUDIT_MAC_STATUS";
    case AUDIT_MAC_CONFIG_CHANGE: return "AUDIT_MAC_CONFIG_CHANGE";
    case AUDIT_MAC_UNLBL_ALLOW: return "AUDIT_MAC_UNLBL_ALLOW";
    case AUDIT_MAC_CIPSOV4_ADD: return "AUDIT_MAC_CIPSOV4_ADD";
    case AUDIT_MAC_CIPSOV4_DEL: return "AUDIT_MAC_CIPSOV4_DEL";
    case AUDIT_MAC_MAP_ADD: return "AUDIT_MAC_MAP_ADD";
    case AUDIT_MAC_MAP_DEL: return "AUDIT_MAC_MAP_DEL";
    case AUDIT_MAC_IPSEC_ADDSA: return "AUDIT_MAC_IPSEC_ADDSA";
    case AUDIT_MAC_IPSEC_DELSA: return "AUDIT_MAC_IPSEC_DELSA";
    case AUDIT_MAC_IPSEC_ADDSPD: return "AUDIT_MAC_IPSEC_ADDSPD";
    case AUDIT_MAC_IPSEC_DELSPD: return "AUDIT_MAC_IPSEC_DELSPD";
    case AUDIT_MAC_IPSEC_EVENT: return "AUDIT_MAC_IPSEC_EVENT";
    case AUDIT_MAC_UNLBL_STCADD: return "AUDIT_MAC_UNLBL_STCADD";
    case AUDIT_MAC_UNLBL_STCDEL: return "AUDIT_MAC_UNLBL_STCDEL";

    case AUDIT_ANOM_PROMISCUOUS: return "AUDIT_ANOM_PROMISCUOUS";
    case AUDIT_ANOM_ABEND: return "AUDIT_ANOM_ABEND";
    case AUDIT_ANOM_LINK: return "AUDIT_ANOM_LINK";
    case AUDIT_INTEGRITY_DATA: return "AUDIT_INTEGRITY_DATA";
    case AUDIT_INTEGRITY_METADATA: return "AUDIT_INTEGRITY_METADATA";
    case AUDIT_INTEGRITY_STATUS: return "AUDIT_INTEGRITY_STATUS";
    case AUDIT_INTEGRITY_HASH: return "AUDIT_INTEGRITY_HASH";
    case AUDIT_INTEGRITY_PCR: return "AUDIT_INTEGRITY_PCR";
    case AUDIT_INTEGRITY_RULE: return "AUDIT_INTEGRITY_RULE";
  }

  if(type >= AUDIT_FIRST_USER_MSG && type <= AUDIT_LAST_USER_MSG)
      return "UNKNOWN AUDIT USER_MSG";

  if(type >= AUDIT_FIRST_USER_MSG2 && type <= AUDIT_LAST_USER_MSG2)
      return "UNKNOWN AUDIT USER_MSG2";

  if(type >= AUDIT_FIRST_KERN_ANOM_MSG && type <= AUDIT_LAST_KERN_ANOM_MSG)
      return "UNKNOWN AUDIT KERN_ANOM_MSG";

  return "UNKNOWN AUDIT MSG";
}
