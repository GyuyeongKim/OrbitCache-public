#include "header.h"

struct arg_t {
  int sock;
	int PROTOCOL_ID; 
	int SERVER_ID; 
  int DIST;
} __attribute__((packed));

/* WORKER */
void *worker_t(void *arg){
	struct arg_t *args = (struct arg_t *)arg;
	int PROTOCOL_ID = args->PROTOCOL_ID;
	int SERVER_ID = args->SERVER_ID;
  int DIST = args->DIST;


	pthread_mutex_lock(&lock_tid);
	int i = t_id++;
  int thread_id = i;
	int global_partition_id = (SERVER_ID-1)*NUM_WORKERS+t_id-1;
  int partition_id = i;


  CountMinSketch cms;
  TopKItem topK[TOP_K_SIZE];
  int topKCount = 0;

  struct sockaddr_in srv_addr;
	memset(&srv_addr, 0, sizeof(srv_addr));
	srv_addr.sin_family = AF_INET;
  if(PROTOCOL_ID == NOCACHE) srv_addr.sin_port = htons(NOCACHE_BASE_PORT+i);
  else if(PROTOCOL_ID == NETCACHE) srv_addr.sin_port = htons(ORBITCACHE_BASE_PORT+i);
  else if (PROTOCOL_ID == ORBITCACHE) srv_addr.sin_port = htons(ORBITCACHE_BASE_PORT+i);
  else{
    printf("PROTOCOL ID is unknown. Cannot assign port numbers.\n");
    exit(1);
  }
	srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	// Creat a socket
	int sock;
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("Could not create socket\n");
		exit(1);
	}

	if ((bind(sock, (struct sockaddr *)&srv_addr, sizeof(srv_addr))) < 0) {
		printf("Could not bind socket\n");
		close(sock);
		exit(1);
	}

  printf("Tx/Rx Worker %d,Global Partition %d is running with Socket %d  \n",i,global_partition_id,sock);
  pthread_mutex_unlock(&lock_tid);
	pin_to_cpu(i);  // CPU Affinity

  /* Data initialization for each partition */




  srand((unsigned int)time(NULL)); 
  tommy_hashlin hashlin;
  tommy_hashlin_init(&hashlin);
  struct kv_hdr kv_node; 
  char keyStr[KEY_SIZE]; 
  uint32_t dstAddr; 
  char* ipaddr = get_ip_address(interface);
  uint64_t hkey_hi = 0;
  uint8_t par_id = 0;
  for(int i = 0; i < DATASET_SIZE; i++) { 
     tostring_key(i,keyStr);
      dstAddr = fnv1_16_str(keyStr)%NUM_SRV;
      if(strcmp(dst_ip[dstAddr],ipaddr) == 0 ){
        hkey_hi = hash64_str(keyStr);
        par_id = (hkey_hi + 0x12345678)%NUM_SRV_WORKERS;
        if( par_id == partition_id) { 
            
       
              if((rand() % 100) >= LRATIO) {
                char valueStr[SMALL_VALUE];
                memset(valueStr, 'A', SMALL_VALUE);
                valueStr[SMALL_VALUE] = '\0'; 
                tommy_put_init(&hashlin, keyStr, valueStr);
              }
              else {
                char valueStr[LARGE_VALUE];
                memset(valueStr, 'A', LARGE_VALUE); 
                valueStr[LARGE_VALUE] = '\0'; 
                tommy_put_init(&hashlin, keyStr, valueStr);
              }
          

        }
      }
  }

  char* value;
  printf("Data preparation done\n");

	struct sockaddr_in cli_addr;
	int cli_addr_len = sizeof(cli_addr);
	int n = 0;
  size_t value_size = 0;
  if (PROTOCOL_ID == NOCACHE){
    struct nocache_hdr RecvBuffer;
    int temp_cache = 0;
    uint64_t start_time = get_cur_ns(); 
    uint32_t rx_counter = 0;
    while(1){

      if((get_cur_ns() - start_time  ) > 1e9 ){
        //printf("Rx Throughput: %u\n", rx_counter);
        rx_counter = 0;
        start_time = get_cur_ns();
      }
      
      if (rx_counter >= LIMIT_RPS) {
          continue;  
      }
        n = recvfrom(sock, &RecvBuffer, sizeof(RecvBuffer), MSG_DONTWAIT, (struct sockaddr*)&(cli_addr), &cli_addr_len);
      if (n > 0) {
          if(RecvBuffer.op == OP_READ){
          /* Check the item size */
          RecvBuffer.op = OP_R_REPLY; 
          value = tommy_get(&hashlin, RecvBuffer.org_key);
          value_size = strlen(value);

         
          strncpy(RecvBuffer.value, value, value_size); 
          RecvBuffer.value[value_size] = '\0'; 
          size_t pkt_size = sizeof(RecvBuffer) - MAX_VALUE_SIZE + value_size; // 

          // 패킷 전송
          sendto(sock, &RecvBuffer, pkt_size, 0, (struct sockaddr *)&(cli_addr), sizeof(cli_addr));
        
        }
        else if(RecvBuffer.op == OP_WRITE){
          RecvBuffer.op = OP_W_REPLY;
          value_size = n - (sizeof(RecvBuffer)- MAX_VALUE_SIZE); // 
    
  
          char valueStr[value_size];
          memset(valueStr, 'A', value_size);
          tommy_put(&hashlin, RecvBuffer.org_key, valueStr);

          size_t pkt_size = sizeof(RecvBuffer) - MAX_VALUE_SIZE; 
          sendto(sock, &RecvBuffer, pkt_size,  0,  (struct sockaddr *)&(cli_addr), sizeof(cli_addr));
        }
          rx_counter++;
      }
    }
  } 
  /* NETCACHE */
  else if (PROTOCOL_ID == NETCACHE){
    struct netcache_hdr RecvBuffer;
    int temp_cache = 0;
    uint64_t start_time = get_cur_ns(); 
    uint32_t rx_counter = 0;
    while(1){

      if((get_cur_ns() - start_time  ) > 1e9 ){
        
        rx_counter = 0;
        start_time = get_cur_ns();
      }
      
      if (rx_counter >= LIMIT_RPS) {
          continue; 
      }
      n = recvfrom(sock, &RecvBuffer, sizeof(RecvBuffer), MSG_DONTWAIT, (struct sockaddr*)&(cli_addr), &cli_addr_len);
      if (n > 0) {
          if(RecvBuffer.op == OP_READ){
          /* Check the item size */
          RecvBuffer.op = OP_R_REPLY;
          value = tommy_get(&hashlin, RecvBuffer.org_key);
          value_size = strlen(value);

        
          strncpy(RecvBuffer.value, value, value_size); 
          RecvBuffer.value[value_size] = '\0'; 
          size_t pkt_size = sizeof(RecvBuffer) - MAX_VALUE_SIZE + value_size; 
        
          sendto(sock, &RecvBuffer, pkt_size, 0, (struct sockaddr *)&(cli_addr), sizeof(cli_addr));
        
        }
        else if(RecvBuffer.op == OP_WRITE){
          RecvBuffer.op = OP_W_REPLY;
          value_size = n - (sizeof(RecvBuffer)- MAX_VALUE_SIZE); 

          char valueStr[value_size];
          memset(valueStr, 'A', value_size);
          tommy_put(&hashlin, RecvBuffer.org_key, valueStr);

          size_t pkt_size = sizeof(RecvBuffer) - MAX_VALUE_SIZE; 
          sendto(sock, &RecvBuffer, pkt_size,  0,  (struct sockaddr *)&(cli_addr), sizeof(cli_addr));

        }
        else if(RecvBuffer.op == OP_FETCH){ 
          
          value_size = n - (sizeof(RecvBuffer)- MAX_VALUE_SIZE);

          RecvBuffer.op = OP_F_REPLY;
          size_t pkt_size = n;
          sendto(sock, &RecvBuffer, pkt_size,  0,  (struct sockaddr *)&(cli_addr), sizeof(cli_addr));

      
          RecvBuffer.op = OP_W_REPLY;
          RecvBuffer.seq = 0;
          char valueStr[value_size];
          memset(valueStr, 'A', value_size);
          tommy_put(&hashlin, RecvBuffer.org_key, valueStr);

          pkt_size = sizeof(RecvBuffer) - MAX_VALUE_SIZE; 
          sendto(sock, &RecvBuffer, pkt_size,  0,  (struct sockaddr *)&(cli_addr), sizeof(cli_addr));

        }
          rx_counter++;
      }
    }
  }
  /* ORBITCACHE */
  else if(PROTOCOL_ID == ORBITCACHE){
      struct orbitcache_hdr RecvBuffer;
      struct client_info CliAddr;
      struct report_hdr ReportBuffer;
      
        /* Hot key report socket definition */
        struct sockaddr_in report_addr;

        memset(&report_addr, 0, sizeof(report_addr)); // Initialize memory space with zeros
        report_addr.sin_family = AF_INET; // IPv4
        report_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        report_addr.sin_port = htons(REPORT_PORT);
        inet_pton(AF_INET, con_addr, &report_addr.sin_addr);
        int report_sock;
        if ((report_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) { 
            printf("Could not create socket\n");
            exit(1);
        }
      if (DYNAMIC){
        
        if (connect(report_sock, (struct sockaddr *)&report_addr, sizeof(report_addr)) < 0) {
            perror("Connect failed!");
            exit(1);
        }
      }
  
      int temp_cache = 0;
      uint64_t start_time = get_cur_ns(); 
      uint64_t start_time_dynamic = get_cur_ns(); 
      uint32_t rx_counter = 0;
      while(1){

        if((get_cur_ns() - start_time  ) > 1e9 ){
          //printf("Rx Throughput: %u\n", rx_counter);
          rx_counter = 0;
          start_time = get_cur_ns();
        }

   
        if (rx_counter >= LIMIT_RPS) {
            continue;  
        }

        if (DYNAMIC){
          /* Hot key report */
          if((get_cur_ns() - start_time_dynamic ) > HOTKEY_REPORT_INTERVAL*1e9 ){
            for (int i = 0; i < topKCount; i++) {
                strncpy(ReportBuffer.key, topK[i].key, sizeof(ReportBuffer.key));
                ReportBuffer.popularity = topK[i].popularity;
                if( ReportBuffer.popularity > 100){
                  size_t pkt_size = sizeof(ReportBuffer);
                  send(report_sock, &ReportBuffer, pkt_size, 0);  
                  //printf("Partition %d reported\n",global_partition_id); 
                }
            }
            //printf("Server %d Sent %d pkts\n",global_partition_id,topKCount);
            resetReport(&cms, topK, &topKCount);
            start_time_dynamic = get_cur_ns();
          }
        }



        n = recvfrom(sock, &RecvBuffer, sizeof(RecvBuffer), MSG_DONTWAIT, (struct sockaddr*)&(cli_addr), &cli_addr_len);
         if (n > 0) {
          if(RecvBuffer.op == OP_READ || RecvBuffer.op == OP_CRCTN){
              RecvBuffer.op = OP_R_REPLY;
              strncpy(keyStr, RecvBuffer.org_key, sizeof(keyStr));
              value = tommy_get(&hashlin, keyStr);
              value_size = strlen(value);
              RecvBuffer.flag = LASTACK; 
              snprintf(RecvBuffer.value, value_size, "%s", value);
              size_t pkt_size = sizeof(RecvBuffer) - MAX_VALUE_SIZE + value_size; 
              // 패킷 전송
              
              sendto(sock, &RecvBuffer, pkt_size, 0, (struct sockaddr *)&(cli_addr), sizeof(cli_addr));
          }
          else if(RecvBuffer.op == OP_FETCH){
              size_t pkt_size = 0;
              RecvBuffer.hkey_hi = htonll(hash64_str(RecvBuffer.org_key)); 
              RecvBuffer.hkey_lo = htonll(hash64_str2(RecvBuffer.org_key)); 
              RecvBuffer.op = OP_F_REPLY;
              strncpy(keyStr, RecvBuffer.org_key, sizeof(keyStr));
              value = tommy_get(&hashlin, keyStr);
              value_size = strlen(value);
              snprintf(RecvBuffer.value, value_size, "%s", value);
              RecvBuffer.flag = FETCHING;
              RecvBuffer.cached = 1;
              pkt_size = sizeof(RecvBuffer) - MAX_VALUE_SIZE + value_size; 
              sendto(sock, &RecvBuffer, pkt_size, 0, (struct sockaddr *)&(cli_addr), sizeof(cli_addr));

              printf("Key %s is fetched\n",RecvBuffer.org_key);
             
          }
          else if(RecvBuffer.op == OP_WRITE){
              RecvBuffer.op = OP_W_REPLY;
              strncpy(keyStr, RecvBuffer.org_key, sizeof(keyStr));
              //keyStr[sizeof(keyStr)] = '\0'; // Ensure null termination
              value_size = n - (sizeof(RecvBuffer)- MAX_VALUE_SIZE);

              char valueStr[value_size];
              memset(valueStr, 'A', value_size);
              tommy_put(&hashlin, keyStr, valueStr);

              size_t pkt_size = 0;
              if(RecvBuffer.flag == FETCHING){
                pkt_size = n;
                RecvBuffer.cached = 1;
                //printf("%u %s %lu %lu\n",i,RecvBuffer.org_key,ntohll(RecvBuffer.hkey_hi),ntohll(RecvBuffer.hkey_lo));
              }
              else pkt_size = sizeof(RecvBuffer)  - MAX_VALUE_SIZE;

              //RecvBuffer.flag = NOFETCH;
              sendto(sock, &RecvBuffer, pkt_size,  0,  (struct sockaddr *)&(cli_addr), sizeof(cli_addr));

          }
          rx_counter++;
          /* Preload hot items for static workload experiments */
          if(temp_cache == 0 && SERVER_ID == 1 && thread_id == 0){
            temp_cache = 1;
            if(!DYNAMIC){
              for(uint32_t i=0;i<NUM_HOTKEY;i++){
                RecvBuffer.op = OP_W_REPLY; 
                tostring_key(i,RecvBuffer.org_key);
                RecvBuffer.hkey_hi = htonll(hash64_str(RecvBuffer.org_key)); 
                RecvBuffer.hkey_lo = htonll(hash64_str2(RecvBuffer.org_key)); 
                
                printf("%u %s %lu %lu\n",i,RecvBuffer.org_key,ntohll(RecvBuffer.hkey_hi),ntohll(RecvBuffer.hkey_lo));
              
                RecvBuffer.flag = FETCHING;
                RecvBuffer.cached = 1; 

              
                if(((double)i/(double)NUM_HOTKEY)*100 >= LRATIO) value_size = SMALL_VALUE;
                else value_size = LARGE_VALUE;
                memset(RecvBuffer.value, 0, value_size); 
                size_t pkt_size = sizeof(RecvBuffer) - MAX_VALUE_SIZE + value_size; 
               
                //sleep(0.01);
                sendto(sock, &RecvBuffer, pkt_size, 0, (struct sockaddr *)&(cli_addr), sizeof(cli_addr));
              }
            }

            printf("The preload of %d cache items are done.\n",NUM_HOTKEY);
          }
           
          if (DYNAMIC){
              /* Key popurarity update */
              if(RecvBuffer.op != OP_FETCH){
                  updateCMS(&cms, RecvBuffer.org_key);
                  updateTopK(&cms, topK, &topKCount, RecvBuffer.org_key);
                } 
            }
         }




      }
		}
 
  close(sock);
  tommy_hashlin_done(&hashlin);
}

int main(int argc, char *argv[]) {
	if ( argc != 2){
	 printf("Input : %s  PROTOCOL_ID\n", argv[0]);
	 exit(1);
	}

	int PROTOCOL_ID = atoi(argv[1]);
  int DIST = 0;
	
	int SERVER_ID = get_server_id(interface)  - NUM_CLI;

	if(SERVER_ID > 255 || SERVER_ID < 1){
    printf("Your server ID is not normal please check your network and server configuration.");
    exit(1);
  }
	printf("Server %d is running\n",SERVER_ID);
  if(PROTOCOL_ID == NETCACHE){
    printf("NetCache is running\n");
    printf("Are you sure that the switch program is NETCACHE? double check it!\n");
  }
  else if(PROTOCOL_ID == ORBITCACHE) printf("OrbitCache is running\n");
  else if (PROTOCOL_ID == NOCACHE) printf("NoCache is running\n");
  else{
    printf("Unknown protocol! Check program arguments again. \n");
    exit(1);
  }

	struct arg_t args;
	args.PROTOCOL_ID = PROTOCOL_ID;
	args.SERVER_ID = SERVER_ID;
	args.DIST = DIST;

     for (int i = 0; i < MAX_WORKERS; i++) {
        pthread_mutex_init(&lock_jobqueue[i], NULL);
    }

	pthread_t dispatcher,worker[MAX_WORKERS]; 
 
  pthread_mutex_lock(&lock_create);
	for(int i=0;i<NUM_WORKERS;i++){
    pthread_create(&worker[i],NULL, worker_t ,(void *)&args); // Worker는 자체 socket을 쓰므로 인자가 필요없다.
	}
  pthread_mutex_unlock(&lock_create);

	for(int i=0;i<NUM_WORKERS;i++) pthread_join(worker[i], NULL);
   

	return 0;
}
