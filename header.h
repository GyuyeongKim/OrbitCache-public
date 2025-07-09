/*
header.h
*/
#define _GNU_SOURCE
#include <endian.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <math.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sched.h>
#include <assert.h>
#include <signal.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <limits.h>
#include <linux/sockios.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <inttypes.h>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>
#include "tommyds/tommyhashlin.h"
#include "tommyds/tommyhashlin.c"
#include "tommyds/tommyhash.h"
#include "tommyds/tommyhash.c"

/* Cluster configuration */
#define DYNAMIC 0 // If you wnat to use dynamic workloads, set this to 1.
#define NUM_HOTKEY 128 // # of cached items
#define EXTRA_WORK 0
#define RUNTIME 0 // 10us = 10000ns
#define POPULARITY_REVERSE 0
#define HOTKEY_REPORT_INTERVAL 1
#define MAX_SRV 8 // # of nodes.
#define NUM_SRV 4 // # of storage servers
#define NUM_CLI 4 // Only need to assign server IDs.
#define MAX_WORKERS 16 // Max. worker numbers
#define NUM_SRV_WORKERS 8// # of threads in a server node
#define NUM_CLI_WORKERS 1 // # of threads in a client node
#define NUM_WORKERS NUM_SRV_WORKERS
#define TRACE 0
#define LRATIO 18// % of large items. 10 means 10%. default: 18%
#define DATASET_SIZE 10000000 // Overall dataset size
//#define LIMIT_RPS 10000000// Dynamic experiments
#define LIMIT_RPS 100000 // Static experiments
//#define DATASET_SIZE 1000 // Overall dataset size
/* Only used for dynamic workload experiments */
#define NUM_HOTKEYS NUM_HOTKEY // Hot-in 실험에서 사용할 키 수 (popularity를 반전시킬 키의 수)

/* Macro for Key-Value pair*/
#define KEY_SIZE 16 // key size.
#define SMALL_VALUE 64
#define LARGE_VALUE 1024
#define MAX_VALUE_SIZE 1416 // Maximum value size

char *interface = "enp1s0np0"; // Currently assume that servers have the same interface name.

// reverse order
char *dst_ip[MAX_SRV] = {
    "10.0.1.108",  // dst_ip[0]
    "10.0.1.107",  // dst_ip[1]
    "10.0.1.106",  // dst_ip[2]
    "10.0.1.105",  // dst_ip[3]
    "10.0.1.104",  // dst_ip[4]
    "10.0.1.103",  // dst_ip[5]
    "10.0.1.102"   // dst_ip[6]
    "10.0.1.101"   // dst_ip[7]
};
char* con_addr ="10.0.1.111"; // switch Controller IP address

/* Static macros. Not need to configure this */
#define QUEUE_SIZE 10000000 // Maximum job queue length
#define BUSY 1
#define IDLE 0
#define ORBITCACHE_BASE_PORT 1000
#define NOCACHE_BASE_PORT 2000
#define NETCACHE_BASE_PORT 1000
#define REPORT_PORT 4321
#define LASTACK 255
/* Lock and other stats stuff */


pthread_mutex_t lock_tid = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock_create = PTHREAD_MUTEX_INITIALIZER;
int t_id = 0;
pthread_mutex_t lock_counter = PTHREAD_MUTEX_INITIALIZER;
uint32_t global_load_counter  = 0;
pthread_mutex_t lock_RACKSCHED = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock_jobqueue[MAX_WORKERS];
pthread_mutex_t lock_stealqueue = PTHREAD_MUTEX_INITIALIZER;
int local_pkt_counter_cache[MAX_WORKERS] = {0,};
int local_pkt_counter_miss[MAX_WORKERS] = {0,};
int local_pkt_counter_server[MAX_WORKERS] = {0,};
int local_pkt_counter[MAX_WORKERS] = {0,};
int global_pkt_counter = 0;
int global_pkt_counter_cache = 0;
int global_pkt_counter_miss = 0;
int global_pkt_counter_server = 0;
int global_par_counter[MAX_SRV*NUM_SRV_WORKERS] ={0,};

pthread_mutex_t lock_txid = PTHREAD_MUTEX_INITIALIZER;
int tx_id = 0;
pthread_mutex_t lock_rxid = PTHREAD_MUTEX_INITIALIZER;
int rx_id = 0;

/* Macro for Protocol */
#define NOCACHE 0
#define NOTUSED 1
#define NETCACHE 2
#define ORBITCACHE 3

/* Operation type Macro */
#define OP_READ 0
#define OP_R_REPLY 1
#define OP_WRITE 2
#define OP_W_REPLY 3
#define OP_CRCTN 4
#define OP_FETCH 5
#define OP_F_REPLY 6
#define FETCHING 1
#define NOFETCH 0
#define MISSED 2

/* Macro for Top-k report and Str key conversion */
#define TOP_K_SIZE 10
#define HASH_FUNCTIONS 5
#define SKETCH_WIDTH 65536
#define CHARSET_SIZE 62

double reverse_pmf[DATASET_SIZE]; // orbitcache만 필요함
double reverse_cdf[DATASET_SIZE];  // orbitcache만 필요함
double pmf[DATASET_SIZE];
double cdf[DATASET_SIZE];


/* Byte ordering stuff for 64-bit data*/
#ifdef WORDS_BIGENDIAN
#define htonll(x)   (x)
#define ntohll(x)   (x)
#else
#define htonll(x)   ((((uint64_t)htonl(x)) << 32) + htonl(x >> 32))
#define ntohll(x)   ((((uint64_t)ntohl(x)) << 32) + ntohl(x >> 32))
#endif
pthread_mutex_t lock_arr = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock_seq = PTHREAD_MUTEX_INITIALIZER;

#pragma pack(1)
struct orbitcache_hdr{ // 224 bits, 28 bytes // 1460(MTU) - 28 = 1432 bytes, 16-B key and 1416-B value
  uint8_t op; // 1 Operation type: Read, Read-reply, Write, Write-reply, Correction
  uint32_t seq; //  4 Sequence number for collision handling and recovery
  uint64_t hkey_hi; // 8 64-bit hash key
  uint64_t hkey_lo; //8 64-bit hash key
  uint8_t flag;  // 1s
  uint8_t cached; // 1 Only need for stats.
  uint32_t latency; // 4 Only need for stats.
  uint8_t par_id; // 1 Partition ID. Only need for stats.
  char org_key[KEY_SIZE]; // (Unhashed) Original Key from client
  char value[MAX_VALUE_SIZE]; // value
} __attribute__((packed));

#pragma pack(1)
struct netcache_hdr{
  uint8_t op; // Operation type: Read, Read-reply, Write, Write-reply, Correction
  uint32_t seq; // Sequence number for collision handling and recovery
  uint64_t hkey_hi; // 64-bit hash key
  uint64_t hkey_lo; // 64-bit hash key
  uint8_t cached; // Only need for stats.
  uint32_t latency; // Only need for stats.
  uint8_t par_id; // Partition ID. Only need for stats.
  char org_key[KEY_SIZE]; // (Unhashed) Original Key from client
  char value[MAX_VALUE_SIZE]; // value
} __attribute__((packed));

#pragma pack(1)
struct nocache_hdr{
  uint8_t op; // Operation type: Read, Read-reply, Write, Write-reply, Correction
  uint32_t seq; // Sequence number for collision handling and recovery
  uint8_t cached; // Only need for stats.
  uint32_t latency; // Only need for stats.
  uint8_t par_id; // Partition ID. Only need for stats.
  char org_key[KEY_SIZE]; // (Unhashed) Original Key from client
  char value[MAX_VALUE_SIZE]; // value
} __attribute__((packed));


#pragma pack(1)
struct client_info {
  struct sockaddr_in cli_addr;
}__attribute__((packed));

#pragma pack(1)
struct report_hdr{
  char key[KEY_SIZE]; // (Unhashed) Original Key from client
  uint32_t popularity;
} __attribute__((packed));


void pin_to_cpu(int core){
	int ret;
	cpu_set_t cpuset;
	pthread_t thread;

	thread = pthread_self();
	CPU_ZERO(&cpuset);
	CPU_SET(core, &cpuset);
	ret = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
  if (ret != 0){
	    printf("Cannot pin thread. may be too many threads? \n");
			exit(1);
    }
}

#define MAX_SEQ ((unsigned long long)50000000 * KEY_SIZE)

char* arr;
uint32_t seq_global;
void initialize_arr() {
    arr = (char*) malloc(MAX_SEQ * sizeof(char));
    if (arr != NULL) {
        memset(arr, 0, MAX_SEQ);
    } else {
        // 메모리 할당 실패 처리
    }
}

#define MAX_SEQ_HDRS 50000000  // Adjust as needed
struct seq_hdr {
    uint32_t seq;
    char org_key[KEY_SIZE];
};
struct seq_hdr seq_hdrs[MAX_SEQ_HDRS];

/* 현재 시간을 nanoseconds 수준으로 측정한다 */
uint64_t get_cur_ns() {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  uint64_t t = ts.tv_sec * 1000 * 1000 * 1000 + ts.tv_nsec;
  return t;
}

/* 현재 시간을 microseconds 수준으로 측정한다 */
uint32_t get_cur_us() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    uint32_t t = ts.tv_sec * 1000 * 1000 + ts.tv_nsec / 1000;
    return t;
}


int get_server_id(char *interface){
    int fd;
    struct ifreq ifr;
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    /* I want to get an IPv4 IP address */
    ifr.ifr_addr.sa_family = AF_INET;
    /* I want IP address attached to interface */
    strncpy(ifr.ifr_name, interface, IFNAMSIZ-1);
    ioctl(fd, SIOCGIFADDR, &ifr);
    close(fd);
		/* display only the last number of the IP address */
		char* ip = inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
    return (ip[strlen(ip) - 1] - '0');
}


char* get_ip_address(char *interface) {
    int fd;
    struct ifreq ifr;
    char* ip_address;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, interface, IFNAMSIZ-1);
    ioctl(fd, SIOCGIFADDR, &ifr);
    close(fd);

    ip_address = inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);

    return strdup(ip_address);
}


#define FNV_16_PRIME ((uint16_t)0x0101)
#define FNV_16_INIT ((uint16_t)0x811c)

uint16_t fnv1_16_str(char *value) {
    uint16_t hash = FNV_16_INIT;

    while (*value) {
        hash *= FNV_16_PRIME;
        hash ^= (uint16_t)(*value);
        value++;
    }

    return hash;
}


uint64_t hash64_str2(const char* str) {
    uint64_t value = 0xABCDEF;
    while (*str) {

        value = (value * 33 + (uint8_t)*str++ + 0x45) & 0xFFFFFFFFFFFFFFFF;
    }

    value = (((value >> 30) ^ value) * 0x123456789ABCDEF1) & 0xFFFFFFFFFFFFFFFF;
    value = (((value >> 33) ^ value) * 0xFEDCBA9876543210) & 0xFFFFFFFFFFFFFFFF;
    value = (value >> 35) ^ value;

    return value & 0xFFFFFFFFFFFFFFFF;
}


uint64_t hash64_str(const char* str) {
    uint64_t value = 0;
    while (*str) {
        value = (value * 31 + (uint8_t)*str++) & 0xFFFFFFFFFFFFFFFF;
    }


    value = (((value >> 32) ^ value) * 0xc4ceb9fe1a85ec53) & 0xFFFFFFFFFFFFFFFF;
    value = (((value >> 32) ^ value) * 0xc4ceb9fe1a85ec53) & 0xFFFFFFFFFFFFFFFF;
    value = (value >> 32) ^ value;

    return value & 0xFFFFFFFFFFFFFFFF;
}

uint64_t hash64(uint64_t value) {
    value = ((value >> 32) ^ value) * 0x1a293fe761c8f5b3;
    value = ((value >> 32) ^ value) * 0x1a293fe761c8f5b3;
    value = (value >> 32) ^ value;
    return value;
}




double fixed_dist(double mean) {
    return mean;
}


void DummyJob() {
    //uint64_t ss = get_cur_ns();

    uint64_t run_ns = RUNTIME;
    uint64_t i = 0;
    do {
        asm volatile ("nop");
        i++;
    } while (i < (double) run_ns);
   //printf("%lu \n",get_cur_ns() - ss);
}


struct QueueNode {
  void* data;
  size_t dataSize;
  struct QueueNode* next;
};

struct Queue {
  struct QueueNode* head;
  struct QueueNode* tail;
  uint32_t size;
};

void queue_init(struct Queue* q) {
  q->head = NULL;
  q->tail = NULL;
  q->size = 0;

}

uint32_t queue_length(struct Queue* q) {
  return q->size;
}

uint32_t queue_is_empty(struct Queue* q) {
  return q->size == 0;
}

uint32_t queue_is_full(struct Queue* q) {
  return q->size == QUEUE_SIZE;
}

void queue_push(struct Queue* q, void* data, size_t data_size) {
  if (queue_is_full(q)) return;


  struct QueueNode* new_node = (struct QueueNode*)malloc(sizeof(struct QueueNode));
  new_node->data = malloc(data_size);
  new_node->dataSize = data_size;
  memcpy(new_node->data, data, data_size);
  new_node->next = NULL;

  if (queue_is_empty(q)) {
    q->head = new_node;
    q->tail = new_node;
  }
	else {
    q->tail->next = new_node;
    q->tail = new_node;
  }
  q->size++;
}

int queue_pop(struct Queue* q, void* data) {
  if (queue_is_empty(q)) return 0;

  struct QueueNode* head = q->head;
  memcpy(data, head->data, head->dataSize);
  free(head->data);
  q->head = head->next;
  if (q->head == NULL) q->tail = NULL;
  q->size--;
  free(head);
  return 1;
}


struct Queue job_queue[MAX_WORKERS];
struct Queue cli_queue[MAX_WORKERS];



const int HASH_SALTS[HASH_FUNCTIONS] = {0xAAAA, 0xBBBB, 0xCCCC, 0xDDDD, 0xEEEE};

typedef struct {
    int count[HASH_FUNCTIONS][SKETCH_WIDTH];
} CountMinSketch;

typedef struct {
    char key[KEY_SIZE];
    int popularity;
} TopKItem;

uint32_t murmurHash3(uint32_t key, uint32_t seed) {
    key ^= key >> 16;
    key *= 0x85ebca6b;
    key ^= key >> 13;
    key *= 0xc2b2ae35;
    key ^= key >> 16;
    return key;
}

int hash2(int functionIndex, char* key, int salt) {
    int hash = 0;

    for (int i = 0; key[i] != '\0'; i++) {
        hash = murmurHash3(hash * 31 + key[i], salt);
    }

    return abs(hash % SKETCH_WIDTH);
}

void updateCMS(CountMinSketch *cms, char* key) {
    for (int i = 0; i < HASH_FUNCTIONS; i++) {
        int idx = hash2(i, key, HASH_SALTS[i]);
        cms->count[i][idx]++;
    }
}

int getPopularity(CountMinSketch *cms, char* key) {
    int minFreq = INT_MAX;
    for (int i = 0; i < HASH_FUNCTIONS; i++) {
        int idx = hash2(i, key, HASH_SALTS[i]);
        minFreq = (cms->count[i][idx] < minFreq) ? cms->count[i][idx] : minFreq;
    }
    return minFreq;
}

void heapify(TopKItem *topK, int topKCount, int i) {
    int smallest = i;
    int left = 2 * i + 1;
    int right = 2 * i + 2;

    if (left < topKCount && topK[left].popularity < topK[smallest].popularity) {
        smallest = left;
    }
    if (right < topKCount && topK[right].popularity < topK[smallest].popularity) {
        smallest = right;
    }

    if (smallest != i) {
        TopKItem temp = topK[i];
        topK[i] = topK[smallest];
        topK[smallest] = temp;

        heapify(topK, topKCount, smallest);
    }
}

void updateTopK(CountMinSketch *cms, TopKItem *topK, int *topKCount, char* key) {
    int freq = getPopularity(cms, key);
    bool found = false;

    for (int i = 0; i < *topKCount; i++) {
        if (strcmp(topK[i].key, key) == 0) {
            topK[i].popularity = freq;
            found = true;
            heapify(topK, *topKCount, i);
            break;
        }
    }

    if (!found && *topKCount < TOP_K_SIZE) {
        strcpy(topK[*topKCount].key, key);
        topK[*topKCount].popularity = freq;
        (*topKCount)++;
        heapify(topK, *topKCount, *topKCount - 1);
    } else if (!found) {
        if (freq > topK[0].popularity) {
            strcpy(topK[0].key, key);
            topK[0].popularity = freq;
            heapify(topK, *topKCount, 0);
        }
    }
}

void printTopK(TopKItem* topK, int topKCount) {
    printf("Top %d Items: \n", topKCount);
    for (int i = 0; i < topKCount; i++) {
        printf("Key: %s, Popularity: %d\n", topK[i].key, topK[i].popularity);
    }
}


void resetReport(CountMinSketch *cms, TopKItem *topK, int *topKCount) {
    memset(cms->count, 0, sizeof(cms->count));
    memset(topK, 0, sizeof(TopKItem) * TOP_K_SIZE);
    *topKCount = 0;
}


void seq_put(uint32_t seq2, const char* org_key) {
    uint32_t index = seq2 % MAX_SEQ_HDRS;
    seq_hdrs[index].seq = seq2;
    strncpy(seq_hdrs[index].org_key, org_key, KEY_SIZE - 1);
    seq_hdrs[index].org_key[KEY_SIZE - 1] = '\0';
}


char* seq_get(uint32_t seq2) {
    uint32_t index = seq2 % MAX_SEQ_HDRS;
    if (seq_hdrs[index].seq != seq2) {
        // Entry invalid or overwritten
        return NULL;
    }
    return seq_hdrs[index].org_key;
}


struct kv_hdr {
    tommy_node node;
    char* key;
    char* value;
} ;


void tommy_put_init(tommy_hashlin* hashlin, const char* key, const char* value) {

    struct kv_hdr* kv = malloc(sizeof(struct kv_hdr));
    if (!kv) {
        perror("Failed to allocate memory for kv_hdr");
        return;
    }

    kv->key = strdup(key);
    kv->value = strdup(value);

    tommy_hash_t hash = tommy_strhash_u32(0, key);
    tommy_hashlin_insert(hashlin, &kv->node, kv, hash);
}

void tommy_put(tommy_hashlin* hashlin, const char* key, const char* value) {
    if (EXTRA_WORK) DummyJob(); // Keep this if needed for your use case

    // Calculate hash for the key
    tommy_hash_t hash = tommy_strhash_u32(0, key);

    // Function to compare keys in the hash table
    int compare(const void* arg, const void* obj) {
        return strcmp((const char*)arg, ((const struct kv_hdr*)obj)->key);
    }

    // Check if the key already exists
    struct kv_hdr* existing_kv = tommy_hashlin_search(hashlin, compare, key, hash);
    if (existing_kv) {
        // Key exists, update the value
        free(existing_kv->value); // Free the old value
        existing_kv->value = strdup(value); // Set the new value
    } else {
        // Key does not exist, insert new key-value pair
        struct kv_hdr* kv = malloc(sizeof(struct kv_hdr));
        if (!kv) {
            perror("Failed to allocate memory for kv_hdr");
            return;
        }

        kv->key = strdup(key);
        kv->value = strdup(value);

        tommy_hashlin_insert(hashlin, &kv->node, kv, hash);

    }
}


char* tommy_get(tommy_hashlin* hashlin, const char* key) {
    if (EXTRA_WORK) DummyJob(); // Keep this if needed for your use case

    tommy_hash_t hash = tommy_strhash_u32(0, key);
    int compare(const void* arg, const void* obj) {
        return strcmp((const char*)arg, ((const struct kv_hdr*)obj)->key);
    }

    struct kv_hdr* kv = tommy_hashlin_search(hashlin, compare, key, hash);
    if (!kv) {
        printf("NULL value returned for key %s Check tommy_get()\n",key);
        return NULL;
    }

    return kv->value;
}

char* tommy_get_init(tommy_hashlin* hashlin, const char* key) {

    tommy_hash_t hash = tommy_strhash_u32(0, key);

    int compare(const void* arg, const void* obj) {
        return strcmp((const char*)arg, ((const struct kv_hdr*)obj)->key);
    }


    struct kv_hdr* kv = tommy_hashlin_search(hashlin, compare, key, hash);
    if (!kv) {
        printf("NULL value returned for key %s Check tommy_get()\n",key);
        return NULL;
    }

    return kv->value;
}

// Calculate the CDF for Zipf distribution
double* precalculate_zipf_cdf(double s) {
    double* cdf = malloc((DATASET_SIZE+1) * sizeof(double));
    double sum = 0.0;

    for (uint32_t i = 1; i <= DATASET_SIZE; i++) {
        sum += 1.0 / pow((double)i, s);
        cdf[i] = sum;
    }

    // Normalize CDF so it goes up to 1
    for (uint32_t i = 1; i <= DATASET_SIZE; i++) {
        cdf[i] /= sum;
    }

    return cdf;
}

// Generate a random number following Zipf distribution
uint32_t zipf_random(double* zipf_cdf) {
    double r = (double)rand() / (double)RAND_MAX;

    // Binary search for index with CDF just above r
    uint32_t low = 1, high = DATASET_SIZE, mid;
    while (low < high) {
        mid = low + ((high - low) >> 1);
        if (r > zipf_cdf[mid]) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }

    return low;
}


void initialize_pmf(double s) {
    double z = 0.0;
    for (int i = 1; i <= DATASET_SIZE; i++) {
        z += 1.0 / pow(i, s);
    }

    for (int i = 1; i <= DATASET_SIZE; i++) {
        pmf[i-1] = (1.0 / pow(i, s)) / z;
    }


    // Compute the cumulative distribution function (CDF)
    cdf[0] = pmf[0];
    for (int i = 1; i < DATASET_SIZE; i++) {
        cdf[i] = cdf[i-1] + pmf[i];
    }
}

uint32_t get_uniform_key() {
    return rand() % DATASET_SIZE;
}


uint32_t get_key() {
    double r = (double)rand() / RAND_MAX;

    // Binary search
    int low = 0, high = DATASET_SIZE - 1;
    while (low <= high) {
        int mid = (low + high) / 2;
        if (r < cdf[mid]) {
            high = mid - 1;
        } else {
            low = mid + 1;
        }
    }
    return low;
}


void initialize_reverse_pmf(double s) {
    double z = 0.0;
    for (int i = 1; i <= DATASET_SIZE; i++) {
        z += 1.0 / pow(i, s);
    }

    for (int i = 1; i <= DATASET_SIZE; i++) {
        reverse_pmf[i-1] = (1.0 / pow(i, s)) / z;
    }

    for (int i = 0; i < NUM_HOTKEYS; i++) {
        double temp = reverse_pmf[i];
        reverse_pmf[i] = reverse_pmf[DATASET_SIZE - NUM_HOTKEYS + i];
        reverse_pmf[DATASET_SIZE - NUM_HOTKEYS + i] = temp;
    }

    reverse_cdf[0] = reverse_pmf[0];
    for (int i = 1; i < DATASET_SIZE; i++) {
        reverse_cdf[i] = reverse_cdf[i-1] + reverse_pmf[i];
    }
}

uint32_t get_reverse_key() {
    double r = (double)rand() / RAND_MAX;

    int low = 0, high = DATASET_SIZE - 1;
    while (low <= high) {
        int mid = (low + high) / 2;
        if (r < reverse_cdf[mid]) {
            high = mid - 1;
        } else {
            low = mid + 1;
        }
    }
    return low;
}

const char CHARSET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

uint64_t complex_transform(uint64_t number) {
    uint64_t transformed = number;
    for (int i = 0; i < 5; ++i) {
        transformed = ((transformed << 3) - transformed + 7) & 0xFFFFFFFFFFFFFFFF;
    }
    return transformed;
}

void tostring_key(uint64_t number, char* key) {
    uint64_t transformed_number = complex_transform(number);
    key[KEY_SIZE - 1] = '\0';

    for (int i = KEY_SIZE - 2; i >= 0; i--) {
        int index = transformed_number % CHARSET_SIZE;
        key[i] = CHARSET[index];
        transformed_number /= CHARSET_SIZE;
    }
}

void printBytes(const char* str) {
    int length = strlen(str) + 1;
    for (int i = 0; i < length; i++) {
        printf("%02x ", (unsigned char)str[i]);
    }
    printf("\n");
}

struct counter_t {
    int index;
    double throughput;
};

int compare(const void *a, const void *b) {
    struct counter_t *counterA = (struct counter_t *)a;
    struct counter_t *counterB = (struct counter_t *)b;
    if (counterA->throughput < counterB->throughput) return 1;
    if (counterA->throughput > counterB->throughput) return -1;
    return 0;
}
