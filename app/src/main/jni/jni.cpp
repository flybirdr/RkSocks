#include <jni.h>
#include "hev-main.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include "Socks5Config.h"
#include "Socks5Server.h"
#include "Logger.h"
#include <yaml.h>

typedef struct ThreadData ThreadData;

struct ThreadData {
    char *path;
    int fd;
};

JavaVM *gJvm;
jobject vpnService;
static pthread_t work_thread;
static pthread_key_t current_jni_env;
std::unique_ptr<R::Socks5Server> socksServer{nullptr};
Socks5Config gSocks5Config;

JNIEXPORT jint

JNICALL JNI_OnLoad(JavaVM *jvm, void *reserved) {
    JNIEnv *env = NULL;
    jint result = -1;

    if (jvm->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        return -1;
    }

    result = JNI_VERSION_1_6;

    gJvm = jvm;


    return result;
}

JNIEXPORT void JNICALL
JNI_OnUnload(JavaVM
             *vm,
             void *reserved
) {
//TODO-
}

extern "C"
JNIEXPORT void JNICALL
Java_com_rookie_r_TunService_setService(JNIEnv
                                        *env,
                                        jobject thiz, jobject
                                        vpn_service) {
    vpnService = env->NewGlobalRef(vpn_service);
}


static void *
thread_handler(void *data) {
    auto *tdata = (ThreadData *) data;

    hev_socks5_tunnel_main(tdata->path, tdata->fd);

    free(tdata->path);
    free(tdata);

    return NULL;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_rookie_r_Tun2Socks_startTun2Socks(JNIEnv
                                           *env,
                                           jobject thiz, jstring
                                           config_path,
                                           jint fd
) {

}

extern "C"
JNIEXPORT void JNICALL
Java_com_rookie_r_Tun2Socks_stopTun2Socks(JNIEnv *env, jobject thiz) {

}

static int
hev_config_parse_tunnel(yaml_document_t *doc, yaml_node_t *base) {
    yaml_node_pair_t *pair;

    if (!base || YAML_MAPPING_NODE != base->type)
        return -1;

    for (pair = base->data.mapping.pairs.start;
         pair < base->data.mapping.pairs.top; pair++) {
        yaml_node_t *node;
        const char *key;

        if (!pair->key || !pair->value)
            break;

        node = yaml_document_get_node(doc, pair->key);
        if (!node || YAML_SCALAR_NODE != node->type)
            break;
        key = (const char *) node->data.scalar.value;

        node = yaml_document_get_node(doc, pair->value);
        if (!node)
            break;

        if (YAML_SCALAR_NODE == node->type) {
            const char *value = (const char *) node->data.scalar.value;

            if (0 == strcmp(key, "mtu"))
                gSocks5Config.mtu = strtoul(value, NULL, 10);
        }
    }

    return 0;
}

static int
hev_config_parse_socks5(yaml_document_t *doc, yaml_node_t *base) {
    yaml_node_pair_t *pair;
    static char _user[256];
    static char _pass[256];
    const char *addr = NULL;
    const char *port = NULL;
    const char *udpm = NULL;
    const char *user = NULL;
    const char *pass = NULL;
    const char *mark = NULL;

    if (!base || YAML_MAPPING_NODE != base->type)
        return -1;

    for (pair = base->data.mapping.pairs.start;
         pair < base->data.mapping.pairs.top; pair++) {
        yaml_node_t *node;
        const char *key, *value;

        if (!pair->key || !pair->value)
            break;

        node = yaml_document_get_node(doc, pair->key);
        if (!node || YAML_SCALAR_NODE != node->type)
            break;
        key = (const char *) node->data.scalar.value;

        node = yaml_document_get_node(doc, pair->value);
        if (!node || YAML_SCALAR_NODE != node->type)
            break;
        value = (const char *) node->data.scalar.value;

        if (0 == strcmp(key, "port"))
            port = value;
        else if (0 == strcmp(key, "address"))
            addr = value;
        else if (0 == strcmp(key, "udp"))
            udpm = value;
        else if (0 == strcmp(key, "username"))
            user = value;
        else if (0 == strcmp(key, "password"))
            pass = value;
        else if (0 == strcmp(key, "mark"))
            mark = value;
    }

    if (!port) {
        fprintf(stderr, "Can't found socks5.port!\n");
        return -1;
    }

    if (!addr) {
        fprintf(stderr, "Can't found socks5.address!\n");
        return -1;
    }

    if ((user && !pass) || (!user && pass)) {
        fprintf(stderr, "Must be set both socks5 username and password!\n");
        return -1;
    }

    gSocks5Config.serverAddr = std::string(addr);
    gSocks5Config.serverPort = strtoul(port, NULL, 10);

    if (user && pass) {
        strncpy(_user, user, 256 - 1);
        strncpy(_pass, pass, 256 - 1);
        gSocks5Config.userName = std::string(_user);
        gSocks5Config.password = std::string(pass);
    }
    return 0;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_rookie_r_TunService_nativeStart(JNIEnv *env, jobject thiz,
                                         jint fd, jstring config_path) {
    jboolean isCopy;
    const char *configPath = env->GetStringUTFChars(config_path, &isCopy);

    yaml_parser_t parser;
    yaml_document_t doc;
    yaml_node_t *root;
    yaml_node_pair_t *pair;
    FILE *fp;
    int res = -1;

    if (!yaml_parser_initialize(&parser))
        goto exit_free_parser;

    fp = fopen(configPath, "r");
    if (!fp) {
        fprintf(stderr, "Open %s failed!\n", configPath);
        goto exit_free_parser;
    }

    yaml_parser_set_input_file(&parser, fp);
    if (!yaml_parser_load(&parser, &doc)) {
        fprintf(stderr, "Parse %s failed!\n", configPath);
        goto exit_close_fp;
    }


    root = yaml_document_get_root_node(&doc);
    if (!root || YAML_MAPPING_NODE != root->type)
        goto exit_close_fp;

    for (pair = root->data.mapping.pairs.start;
         pair < root->data.mapping.pairs.top; pair++) {
        yaml_node_t *node;
        const char *key;
        int res = 0;

        if (!pair->key || !pair->value)
            break;

        node = yaml_document_get_node(&doc, pair->key);
        if (!node || YAML_SCALAR_NODE != node->type)
            break;

        key = (const char *) node->data.scalar.value;
        node = yaml_document_get_node(&doc, pair->value);

        if (0 == strcmp(key, "tunnel"))
            res = hev_config_parse_tunnel(&doc, node);
        else if (0 == strcmp(key, "socks5"))
            res = hev_config_parse_socks5(&doc, node);

        if (res < 0)
            goto exit_close_fp;
    }
    yaml_document_delete(&doc);
    exit_close_fp:
    fclose(fp);
    exit_free_parser:
    yaml_parser_delete(&parser);

    socksServer = std::make_unique<R::Socks5Server>(gSocks5Config);
    socksServer->run();

    const jbyte *bytes;
    ThreadData *tdata;

    if (work_thread)
        return;
    tdata = (ThreadData *) malloc(sizeof(ThreadData));
    tdata->fd = fd;
    bytes = (const jbyte *) env->GetStringUTFChars(config_path, NULL);
    tdata->path = strdup((const char *) bytes);
    env->ReleaseStringUTFChars(config_path, (const char *) bytes);
    pthread_create(&work_thread, NULL, thread_handler, tdata);
}
extern "C"
JNIEXPORT void JNICALL
Java_com_rookie_r_TunService_nativeStop(JNIEnv *env, jobject thiz) {
    if (!work_thread)
        return;
    hev_socks5_tunnel_quit();
    pthread_join(work_thread, nullptr);
    work_thread = 0;
    if (socksServer) {
        socksServer->quit();
    }
    socksServer.reset(nullptr);
}