// client.c //
#include "header.h" 
//tommy_hashlin hashlin;

struct arg_t {
  int sock; 
  struct sockaddr_in srv_addr; 
  uint64_t TARGET_QPS; 
  uint64_t NUM_REQUESTS; 
  int WRATIO;
	int PROTOCOL_ID; 
	int SERVER_ID; 
	int DIST; 
  uint64_t TIME_EXP;
} __attribute__((packed));

void *tx_t(void *arg){ 


  pthread_mutex_lock(&lock_txid);
	int i = tx_id++; // Assign a thread ID. We need to use a global variable to assign it, so we use a lock.
  pin_to_cpu(i*2);  // Since Tx and Rx are a pair, assign them like this: Tx0: Core 0 Rx0: Core 1 Tx1: Core 2 Rx2: Core 3
	pthread_mutex_unlock(&lock_txid);
	

  struct arg_t *args = (struct arg_t *)arg;
  struct sockaddr_in srv_addr = args->srv_addr;
  int sock = args->sock;
  int PROTOCOL_ID = args->PROTOCOL_ID;
	int DIST = args->DIST;
  int WRATIO = args->WRATIO;
  uint64_t SERVER_ID = args->SERVER_ID;

  struct kv_hdr kv_node; 
  char keyStr[KEY_SIZE];
	
	printf("Tx Worker %d is running with Socket %d, CPU %d\n",i,sock,i*2);


  uint64_t NUM_REQUESTS = args->NUM_REQUESTS/NUM_CLI_WORKERS;
  //uint64_t TARGET_QPS = RAND_MAX*args->TARGET_QPS/1000/2/NUM_CLI_WORKERS;
  uint64_t counter = 0; // Tx 통계를 위한 카운터
	uint64_t TARGET_QPS = args->TARGET_QPS/NUM_CLI_WORKERS; 

  srand(time(NULL)); 
  uint64_t elapsed_time = get_cur_ns(); 
	uint64_t temp_time = get_cur_ns(); 
  uint64_t inter_arrival_time = 0; 
  uint64_t start_time = get_cur_ns(); 

 
  uint32_t key_value;
  uint32_t dstAddr = 0;
  size_t value_size = 0;


  const gsl_rng_type * T;
  gsl_rng * r;
  gsl_rng_env_setup();
  T = gsl_rng_default;
  r = gsl_rng_alloc(T);

  double lambda = TARGET_QPS * 1e-9;
  double mu = 1.0/ lambda;

	while(1){
    /*Packet inter-arrival time 을 Exponentional하게 보내기 위한 연산 과정들이다 */
    inter_arrival_time = (uint64_t)(gsl_ran_exponential(r, mu));
    temp_time+=inter_arrival_time;

    // If the inter-inter_arrival_time time has not passed, it waits in an infinite loop.
    while (get_cur_ns() < temp_time)
      ; 
    if(PROTOCOL_ID == NOCACHE){
        struct nocache_hdr SendBuffer={0,}; 
        if(DIST > 0) key_value = get_key();  // If DIST is not 0 (i.e., skewed), generate keys according to the distribution
        else key_value = get_uniform_key(); // If DIST is 0, generate keys uniformly
        tostring_key(key_value,SendBuffer.org_key); // Since the key is generated as a number, convert it to a string.

        uint64_t hkey_temp = hash64_str(SendBuffer.org_key); 
        dstAddr = fnv1_16_str(SendBuffer.org_key)%NUM_SRV;
        SendBuffer.par_id =  (hkey_temp + 0x12345678)% NUM_SRV_WORKERS;

        srv_addr.sin_port = htons(NOCACHE_BASE_PORT+SendBuffer.par_id);
        inet_pton(AF_INET, dst_ip[dstAddr], &srv_addr.sin_addr);
        SendBuffer.latency = get_cur_us(); 
        size_t pkt_size = 0;
        if((rand() % 100) >= WRATIO){
          SendBuffer.op = OP_READ;
          pkt_size = sizeof(SendBuffer) - MAX_VALUE_SIZE; 
        }
        else{ // Write
    
          if((rand() % 100) >= LRATIO)  value_size = SMALL_VALUE;
          else value_size = LARGE_VALUE;
          SendBuffer.op = OP_WRITE;
          
          memset(SendBuffer.value, 0, value_size); 
          SendBuffer.value[value_size] = '\0'; 
          pkt_size = sizeof(SendBuffer) - MAX_VALUE_SIZE + value_size; 
        }
        sendto(sock, &SendBuffer, pkt_size,  0, (struct sockaddr*)&(srv_addr), sizeof(srv_addr));
        
     }
 
    else if(PROTOCOL_ID == NETCACHE){
        struct netcache_hdr SendBuffer={0,}; 
        if(DIST > 0) key_value = get_key();  
        else key_value = get_uniform_key(); 
       
        tostring_key(key_value,SendBuffer.org_key); 


        SendBuffer.hkey_hi = hash64_str(SendBuffer.org_key);
        SendBuffer.hkey_lo = hash64_str2(SendBuffer.org_key); 

      
        dstAddr = fnv1_16_str(SendBuffer.org_key)%NUM_SRV;
        SendBuffer.par_id =  (SendBuffer.hkey_hi + 0x12345678)% NUM_SRV_WORKERS;

 
        SendBuffer.hkey_hi = htonll(SendBuffer.hkey_hi); 
        SendBuffer.hkey_lo = htonll(SendBuffer.hkey_lo); 
        srv_addr.sin_port = htons(NETCACHE_BASE_PORT+SendBuffer.par_id); 
        inet_pton(AF_INET, dst_ip[dstAddr], &srv_addr.sin_addr); 
        SendBuffer.latency = get_cur_us(); // latency 측정용 timestamp
        size_t pkt_size = 0;
        
   
        if((rand() % 100) >= WRATIO){
          SendBuffer.op = OP_READ;
          pkt_size = sizeof(SendBuffer) - MAX_VALUE_SIZE; 
        }
        else{ // Write

        
          if((rand() % 100) >= LRATIO)  value_size = SMALL_VALUE;
          else value_size = LARGE_VALUE;
          SendBuffer.op = OP_WRITE;
          
          memset(SendBuffer.value, 0, value_size); 
          SendBuffer.value[value_size] = '\0';
          pkt_size = sizeof(SendBuffer) - MAX_VALUE_SIZE + value_size; 
        }
        sendto(sock, &SendBuffer, pkt_size,  0, (struct sockaddr*)&(srv_addr), sizeof(srv_addr));
        
     }
    else if(PROTOCOL_ID == ORBITCACHE){
      struct orbitcache_hdr SendBuffer={0,};
        /*
      if (POPULARITY_REVERSE == 0){ // STATIC인 경우
        if(DIST > 0) key_value = get_key();  // DIST 1~3: skewed
        else key_value = get_uniform_key();
      }
      else{
        if (((get_cur_ns() - start_time) / (uint64_t)10e9) % 2 == 0) {
            // 10초 간격으로 get_key()와 get_reverse_key()를 번갈아 호출합니다.
            if (DIST > 0) key_value = get_key(); // DIST 1~3: skewed
            else key_value = get_uniform_key();
        } else {
            if (DIST > 0) key_value = get_reverse_key(); // DIST 1~3: skewed
            else key_value = get_uniform_key();
        }
      }
      */
      if (DIST > 0) key_value = get_key(); // DIST 1~3: skewed
      else key_value = get_uniform_key();
      //key_value = 0;
      //key_value = rand()%NUM_HOTKEY;
      //printf("%d\n",key_value);
      tostring_key(key_value,SendBuffer.org_key); 
      SendBuffer.hkey_hi = hash64_str(SendBuffer.org_key); 
      SendBuffer.hkey_lo = hash64_str2(SendBuffer.org_key); 
      dstAddr = fnv1_16_str(SendBuffer.org_key)%NUM_SRV;
      SendBuffer.par_id =  (SendBuffer.hkey_hi + 0x12345678)% NUM_SRV_WORKERS;
      SendBuffer.hkey_hi = htonll(SendBuffer.hkey_hi);
      SendBuffer.hkey_lo = htonll(SendBuffer.hkey_lo); 
      srv_addr.sin_port = htons(ORBITCACHE_BASE_PORT+SendBuffer.par_id);

  
      //pthread_mutex_lock(&lock_seq);
     // pthread_mutex_lock(&lock_seq);
      seq_put(seq_global, SendBuffer.org_key);
      SendBuffer.seq = htonl(seq_global);
      seq_global++;
      //pthread_mutex_unlock(&lock_seq);


      //pthread_mutex_unlock(&lock_seq);
      //char* org_key = seq_get(&hashlin,seq);
      //printf("%s\n",org_key);
      //strncpy(&arr[seq], SendBuffer.org_key, KEY_SIZE);
      //printf("%d %s %s\n",seq, &arr[seq],SendBuffer.org_key); // OK 정상적임.
      //if(seq + KEY_SIZE <= MAX_SEQ) seq+=KEY_SIZE;
      //else seq = 0;

      inet_pton(AF_INET, dst_ip[dstAddr], &srv_addr.sin_addr);
      SendBuffer.latency = get_cur_us(); // latency  timestamp

      size_t pkt_size = 0;
   
      if((rand() % 100) >= WRATIO){ // Read
        SendBuffer.op = OP_READ;
        pkt_size = sizeof(SendBuffer) - MAX_VALUE_SIZE; 
      }
      else{ // Write
         if((rand() % 100) >= LRATIO){  // Small item
            value_size = SMALL_VALUE;
         }
         else{  // Large item
            value_size = LARGE_VALUE;
         }
        SendBuffer.op = OP_WRITE;
        memset(SendBuffer.value, 0, value_size); 
        SendBuffer.value[value_size] = '\0'; 
        pkt_size = sizeof(SendBuffer) - MAX_VALUE_SIZE + value_size; 
        
      }
      sendto(sock, &SendBuffer, pkt_size,  0, (struct sockaddr*)&(srv_addr), sizeof(srv_addr));
    }
    counter++;
    if(counter >= NUM_REQUESTS) break; 
	}
 
    gsl_rng_free(r);

	double tot_time = (get_cur_ns() - elapsed_time )/1e9;
	double throughput = counter  / tot_time ;
	printf("Tx Worker %d done with %lu reqs, Tx throughput: %d RPS \n",i, counter,(int)throughput);
}

void *rx_t(void *arg){

  pthread_mutex_lock(&lock_rxid);
	int i = rx_id++;
  pin_to_cpu(i*2+1); 
	pthread_mutex_unlock(&lock_rxid);
  
  struct kv_hdr kv_node; 
  char keyStr[KEY_SIZE]; 


  struct arg_t *args = (struct arg_t *)arg;

  struct sockaddr_in cli_addr;
  int cli_addr_len = sizeof(cli_addr);
  int counter=0;
	int sock = args->sock;
	printf("Rx Worker %d is running with Socket %d, CPU %d\n",i,sock,i*2+1);
	uint64_t NUM_REQUESTS = args->NUM_REQUESTS;
  uint64_t TIME_EXP = args->TIME_EXP;
	uint64_t TARGET_QPS = args->TARGET_QPS;
	uint64_t PROTOCOL_ID = args->PROTOCOL_ID;
	uint64_t SERVER_ID = args->SERVER_ID;
	uint64_t DIST = args->DIST;


	char log_file_name[50];
	sprintf(log_file_name,"./log-%lu-%lu-%d-%d-%d-%lu-%lu-%lu-%d.txt",PROTOCOL_ID,SERVER_ID,i,NUM_SRV,NUM_SRV_WORKERS,DIST,TIME_EXP,TARGET_QPS,NUM_HOTKEY);  // log-ServerID-ThreadID-Protocol-REQUESTS-QPS
	FILE* fd;
	if ((fd = fopen(log_file_name, "w")) == NULL) {
		exit(1);
	}

	sprintf(log_file_name,"./log-%lu-%lu-%d-%d-%d-%lu-%lu-%lu-%d-cache.txt",PROTOCOL_ID,SERVER_ID,i,NUM_SRV,NUM_SRV_WORKERS,DIST,TIME_EXP,TARGET_QPS,NUM_HOTKEY);  // log-ServerID-ThreadID-Protocol-REQUESTS-QPS
	FILE* fd_cache;
	if ((fd_cache = fopen(log_file_name, "w")) == NULL) {
		exit(1);
	}

	sprintf(log_file_name,"./log-%lu-%lu-%d-%d-%d-%lu-%lu-%lu-%d-server.txt",PROTOCOL_ID,SERVER_ID,i,NUM_SRV,NUM_SRV_WORKERS,DIST,TIME_EXP,TARGET_QPS,NUM_HOTKEY);  // log-ServerID-ThreadID-Protocol-REQUESTS-QPS
	FILE* fd_server;
	if ((fd_server = fopen(log_file_name, "w")) == NULL) {
		exit(1);
	}

  uint32_t dstAddr =0;
  uint32_t key_value = 0;
  uint64_t elapsed_time = get_cur_ns();

	int n = 0;
  uint32_t collision_counter = 0;
  uint64_t start_time = get_cur_ns(); 
  uint32_t rx_counter = 0;
  uint32_t rx_time = 1;

	uint64_t timer = get_cur_ns();
  while(1){
    if((get_cur_ns() - start_time  ) > 1e9 ){
        printf("%d %u\n", rx_time, rx_counter);
        rx_counter = 0;
        rx_time++;
        start_time = get_cur_ns();
    }
		if((get_cur_ns() - timer  ) > 1e9 ) break;

    if(PROTOCOL_ID == NOCACHE){
      struct sockaddr_in srv_addr = args->srv_addr;
      struct nocache_hdr RecvBuffer;
      int n = recvfrom(sock, &RecvBuffer, sizeof(RecvBuffer), MSG_DONTWAIT, (struct sockaddr*)&(cli_addr), &cli_addr_len);
      if(n>0){
        if(RecvBuffer.op == OP_R_REPLY){
          fprintf(fd,"%u\n",get_cur_us() - RecvBuffer.latency);
            //fprintf(fd_server,"%u %u\n",get_cur_us() - RecvBuffer.latency, RecvBuffer.par_id);
            fprintf(fd_server,"%u\n",get_cur_us() - RecvBuffer.latency);
            local_pkt_counter_server[i]++; 
            global_par_counter[NUM_SRV_WORKERS*(fnv1_16_str(RecvBuffer.org_key)%NUM_SRV)+RecvBuffer.par_id]++;

            local_pkt_counter[i]++; 
            //rx_counter++;

        }
        else if(RecvBuffer.op == OP_W_REPLY){
            fprintf(fd,"%u\n",get_cur_us() - RecvBuffer.latency);
            //fprintf(fd_server,"%u %u\n",get_cur_us() - RecvBuffer.latency, RecvBuffer.par_id);
            //fprintf(fd_server,"%u\n",get_cur_us() - RecvBuffer.latency);
            local_pkt_counter_server[i]++; 
            global_par_counter[NUM_SRV_WORKERS*(fnv1_16_str(RecvBuffer.org_key)%NUM_SRV)+RecvBuffer.par_id]++;
            local_pkt_counter[i]++; 
        }
        rx_counter++;
        timer = get_cur_ns(); 
      }

    }
    else if(PROTOCOL_ID == NETCACHE){
      struct sockaddr_in srv_addr = args->srv_addr;
      struct netcache_hdr RecvBuffer;
      int n = recvfrom(sock, &RecvBuffer, sizeof(RecvBuffer), MSG_DONTWAIT, (struct sockaddr*)&(cli_addr), &cli_addr_len);
      if(n>0){
        if(RecvBuffer.op == OP_R_REPLY || RecvBuffer.op == OP_W_REPLY){
          fprintf(fd,"%u\n",get_cur_us() - RecvBuffer.latency);
          if(RecvBuffer.cached > 0){
            //fprintf(fd_cache,"%u %u\n",get_cur_us() - RecvBuffer.latency,RecvBuffer.cnt);
            fprintf(fd_cache,"%u\n",get_cur_us() - RecvBuffer.latency);
            local_pkt_counter_cache[i]++; 
          }
          else{
            //fprintf(fd_server,"%u %u %u\n",get_cur_us() - RecvBuffer.latency, RecvBuffer.par_id,RecvBuffer.cnt);
            fprintf(fd_server,"%u\n",get_cur_us() - RecvBuffer.latency);
            local_pkt_counter_server[i]++;
            global_par_counter[NUM_SRV_WORKERS*(fnv1_16_str(RecvBuffer.org_key)%NUM_SRV)+RecvBuffer.par_id]++;
          }
            local_pkt_counter[i]++; 
            //rx_counter++;
            //timer = get_cur_ns(); 
        }
      
        rx_counter++;
        timer = get_cur_ns(); 
    }
  }
    else if(PROTOCOL_ID == ORBITCACHE){
      struct sockaddr_in srv_addr = args->srv_addr;
      struct orbitcache_hdr RecvBuffer;
      int n = recvfrom(sock, &RecvBuffer, sizeof(RecvBuffer), MSG_DONTWAIT, (struct sockaddr*)&(cli_addr), &cli_addr_len);
      if(n>0){
          rx_counter++;
          if(RecvBuffer.op == OP_R_REPLY){
            local_pkt_counter[i]++;
            fprintf(fd,"%u\n",get_cur_us() - RecvBuffer.latency);
            if(RecvBuffer.cached == 1){
              local_pkt_counter_cache[i]++; 
              fprintf(fd_cache,"%u\n",get_cur_us() - RecvBuffer.latency);
              uint32_t seq2 = ntohl(RecvBuffer.seq);
              char* client_key = seq_get(seq2);
              // Hash coliision detection and recovery
              if (strncmp(client_key, RecvBuffer.org_key, KEY_SIZE) != 0) { 
                  //printf("Collision detected!%u %s %s\n",seq2,RecvBuffer.org_key,client_key); 
                  collision_counter++; 
                  struct orbitcache_hdr SendBuffer={0,};
                  strncpy(SendBuffer.org_key, client_key, KEY_SIZE);
                  //seq_put(&hashlin,seq2,SendBuffer.org_key);
                  SendBuffer.hkey_hi = hash64_str(SendBuffer.org_key); 
                  SendBuffer.hkey_lo = hash64_str2(SendBuffer.org_key); 
                  dstAddr = fnv1_16_str(SendBuffer.org_key)%NUM_SRV;
                  SendBuffer.par_id =  (SendBuffer.hkey_hi + 0x12345678)% NUM_SRV_WORKERS;
                  SendBuffer.hkey_hi = htonll(SendBuffer.hkey_hi); 
                  SendBuffer.hkey_lo = htonll(SendBuffer.hkey_lo); 
                  srv_addr.sin_port = htons(ORBITCACHE_BASE_PORT+SendBuffer.par_id);
                  SendBuffer.op = OP_CRCTN;
                  SendBuffer.seq = htonl(seq2);
                  inet_pton(AF_INET, dst_ip[dstAddr], &srv_addr.sin_addr); 
                  SendBuffer.latency = get_cur_us(); 
                  size_t pkt_size = sizeof(SendBuffer) - MAX_VALUE_SIZE; 
                  sendto(sock, &SendBuffer, pkt_size,  0, (struct sockaddr*)&(srv_addr), sizeof(srv_addr));
              }
            }
            else{
              fprintf(fd_server,"%u\n",get_cur_us() - RecvBuffer.latency);
              local_pkt_counter_server[i]++;
              global_par_counter[NUM_SRV_WORKERS*(fnv1_16_str(RecvBuffer.org_key)%NUM_SRV)+RecvBuffer.par_id]++;
              if(RecvBuffer.cached == MISSED) local_pkt_counter_miss[i]++; 
            }
        }
          else if(RecvBuffer.op == OP_W_REPLY){
            local_pkt_counter[i]++; 
            fprintf(fd,"%u\n",get_cur_us() - RecvBuffer.latency);
            //fprintf(fd_server,"%u %u %u\n",get_cur_us() - RecvBuffer.latency, RecvBuffer.par_id,RecvBuffer.cnt);
            fprintf(fd_server,"%u\n",get_cur_us() - RecvBuffer.latency);
            local_pkt_counter_server[i]++; 
            global_par_counter[NUM_SRV_WORKERS*(fnv1_16_str(RecvBuffer.org_key)%NUM_SRV)+RecvBuffer.par_id]++;
            //rx_counter++;
            //timer = get_cur_ns(); 
          }
          
          timer = get_cur_ns(); 
      }

    }
  }


	double tot_time = ((get_cur_ns() - elapsed_time )/1e9)-1;
	fprintf(fd,"%f\n",tot_time); 
	printf("Rx Worker %d finished with %u hash collisions\n",i,collision_counter);
	close(args->sock); 
	fclose(fd); 
}

int main(int argc, char *argv[]) {
	if ( argc != 6){
	 printf("%s Usage: ./client PROTOCOL_ID KEY_DIST TIME_EXP TARGET_QPS WRITE_RATIO\n", argv[0]);
	 exit(1);
	}


  pthread_mutex_lock(&lock_seq);
  seq_global = 0;
  pthread_mutex_unlock(&lock_seq);
	int PROTOCOL_ID = atoi(argv[1]);
	int DIST = atoi(argv[2]);
  int TIME_EXP = atoi(argv[3]);
  uint64_t TARGET_QPS = atoi(argv[4]); 
  int WRATIO = atoi(argv[5]);
  int NUM_REQUESTS = TARGET_QPS*TIME_EXP;
  if(DIST > 3 || DIST < 0){
    printf("Distribution cannot exceed 3. 0: uniform, 1: 0.9, 2: 0.95, 3: 0.99\n");
    exit(1);
  }
  if(WRATIO > 100 || WRATIO < 0){
    printf("Write ratio should be 0 <= WRATIO <= 100\n");
    exit(1);
  }
 
/* Automatically get the server ID. After reading the IP address of the interface used for the experiment, use the last number as the ID.
In other words, the IP address must end in 1 2 3 4 5 6 7 8 9 .. If there are more than 10 servers, the function may be changed.
Because, with the current function, if the last IP is 110, it will output 0, not 10.
*/
	int SERVER_ID = get_server_id(interface) ;
  get_server_id(interface);

  if(SERVER_ID > 255 || SERVER_ID == -1 || SERVER_ID == 0){
    printf("Your server ID is not normal please check your network and server configuration.");
    exit(1);
  }
	else printf("Client %d is running \n",SERVER_ID);

  printf("Collision detection table done\n");
 
  srand((unsigned int)time(NULL)); 
 
  if(DIST == 1) {
      initialize_pmf(0.9);               
      initialize_reverse_pmf(0.9);     
  } else if(DIST == 2) {
      initialize_pmf(0.95);              
      initialize_reverse_pmf(0.95);      
  } else if(DIST == 3) {
      initialize_pmf(0.99);             
      initialize_reverse_pmf(0.99);      
  }
  printf("Query PMF setup done\n");


  struct sockaddr_in srv_addr;
  memset(&srv_addr, 0, sizeof(srv_addr)); // Initialize memory space with zeros
  srv_addr.sin_family = AF_INET; // IPv4
	srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  struct arg_t args[MAX_WORKERS]; 
	int sock[MAX_WORKERS]; 
  for(int i=0;i<NUM_CLI_WORKERS;i++){ 
    srv_addr.sin_port = htons(ORBITCACHE_BASE_PORT+i);
		if ((sock[i] = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { 
			printf("Could not create socket\n");
			exit(1);
		}
    if ((bind(sock[i], (struct sockaddr *)&srv_addr, sizeof(srv_addr))) < 0) {
      printf("Could not bind socket\n");
      close(sock[i]);
      exit(1);
    }

		
	  args[i].sock = sock[i];
	  args[i].srv_addr = srv_addr;
	  args[i].TARGET_QPS = TARGET_QPS;
	  args[i].NUM_REQUESTS = NUM_REQUESTS;
    args[i].WRATIO = WRATIO;
	  args[i].PROTOCOL_ID = PROTOCOL_ID;
	  args[i].SERVER_ID = SERVER_ID;
	  args[i].DIST = DIST;
    args[i].TIME_EXP=TIME_EXP;
	}


  pthread_t tx[MAX_WORKERS],rx[MAX_WORKERS]; 

	uint64_t elapsed_time = get_cur_ns(); 
  
  //tommy_hashlin_init(&hashlin);
  for(int i=0;i<NUM_CLI_WORKERS;i++){
		pthread_mutex_lock(&lock_create); 
    pthread_create(&rx[i],NULL, rx_t ,(void *)&args[i]); 
		pthread_create(&tx[i],NULL, tx_t ,(void *)&args[i]); 
		pthread_mutex_unlock(&lock_create); 
  	}

	for(int i=0;i<NUM_CLI_WORKERS;i++){
		pthread_join(rx[i], NULL); 
		pthread_join(tx[i], NULL);
	}


	for(int i=0;i<NUM_CLI_WORKERS;i++){



  global_pkt_counter += local_pkt_counter[i]; 
  global_pkt_counter_cache += local_pkt_counter_cache[i];
  global_pkt_counter_miss += local_pkt_counter_miss[i]; 
  global_pkt_counter_server += local_pkt_counter_server[i]; 
  }
	double tot_time = ((get_cur_ns() - elapsed_time )/1e9)-1; 
	double throughput = global_pkt_counter  / tot_time ; 
	double throughput_cache = global_pkt_counter_cache  / tot_time ; 
  double throughput_server = global_pkt_counter_server  / tot_time ; 
  double lossrate = 1 - ((double)global_pkt_counter / (TARGET_QPS * TIME_EXP));
  double missrate =  ((double)global_pkt_counter_miss / (double)(global_pkt_counter_cache+global_pkt_counter_miss));


     /*
  printf("Total time: %f seconds \n", tot_time);
  printf("Total received pkts: %d \n", global_pkt_counter);
  printf("Packet loss rate: %f \n", lossrate*100);
	printf("Rx Throughput (Total): %d RPS \n", (int)throughput);
	printf("Rx Throughput (Cache): %d RPS \n", (int)throughput_cache);
	printf("Rx Throughput (Server): %d RPS \n", (int)throughput_server);
  double throughput_par=0;
  for(int i=0;i<NUM_SRV_WORKERS*NUM_SRV;i++){
    throughput_par = global_par_counter[i] / tot_time;
    printf("Rx Throughput (Partition%d): %d RPS \n", i,(int) throughput_par);
  } 
   */
   

  struct counter_t counters[NUM_SRV_WORKERS * NUM_SRV];
  for (int i = 0; i < NUM_SRV_WORKERS * NUM_SRV; i++) {
      counters[i].index = i;
      counters[i].throughput = global_par_counter[i] / tot_time;
  }

 
  qsort(counters, NUM_SRV_WORKERS * NUM_SRV, sizeof(struct counter_t), compare);


  printf("%f\n", tot_time); 
  printf("%d\n", global_pkt_counter); 
  printf("%d\n", global_pkt_counter_cache); 
  printf("%d\n", global_pkt_counter_miss); 
  printf("%d\n", global_pkt_counter_server); 
  printf("%f\n", missrate*100); 
  printf("%f\n", lossrate*100);
	printf("%d\n", (int)throughput);
	printf("%d\n", (int)throughput_cache); 
	printf("%d\n", (int)throughput_server);
  

  
  for (int i = 0; i < NUM_SRV_WORKERS * NUM_SRV; i++) {
      printf("%d\n", (int)counters[i].throughput);
  }
 
	return 0;
}
