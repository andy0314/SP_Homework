#include "header.h"

movie_profile* movies[MAX_MOVIES];
unsigned int num_of_movies = 0;
unsigned int num_of_reqs = 0;

// Global request queue and pointer to front of queue
// TODO: critical section to protect the global resources
request* reqs[MAX_REQ];
int front = -1;

/* Note that the maximum number of processes per workstation user is 512. * 
 * We recommend that using about <256 threads is enough in this homework. */
pthread_t tid[MAX_CPU][MAX_THREAD]; //tids for multithread

#ifdef PROCESS
pid_t pid[MAX_CPU][MAX_THREAD]; //pids for multiprocess
#endif


//mutex
pthread_mutex_t lock; 

void initialize(FILE* fp);
request* read_request();
int pop();

typedef struct sort_arg{
	int l, r, layer;
	char** movies;
	double* scores;
}sort_arg;

typedef struct {
	pthread_mutex_t mlock;
	char buf[MAX_MOVIES];
}buftype;

void* merge_sort(void* arg){
	sort_arg arg_self = *((sort_arg*)arg);
	int mid = (arg_self.r + arg_self.l)/2;
	if(arg_self.layer == 3){
		char** movies_left = &arg_self.movies[arg_self.l];
		double* scores_left = &arg_self.scores[arg_self.l];
		char** movies_right = &arg_self.movies[mid];
		double* scores_right = &arg_self.scores[mid];
		sort(movies_right, scores_right, arg_self.r - mid);
		sort(movies_left, scores_left, mid - arg_self.l);
	}
	else{
		sort_arg arg_left;
		arg_left.l = arg_self.l;
		arg_left.r = mid;
		arg_left.movies = arg_self.movies;
		arg_left.scores = arg_self.scores;
		arg_left.layer = arg_self.layer + 1;

		sort_arg arg_right;
		arg_right.l = mid;
		arg_right.r = arg_self.r;
		arg_right.movies = arg_self.movies;
		arg_right.scores = arg_self.scores;
		arg_right.layer = arg_self.layer + 1;

		pthread_t tid_left, tid_right;

		pthread_create(&tid_left, NULL, merge_sort, (void*)&arg_left);
		pthread_create(&tid_right, NULL, merge_sort, (void*)&arg_right);

		pthread_join(tid_left, NULL);
		pthread_join(tid_right, NULL);
	}
	double* temp_score = (double*)malloc(((arg_self.r - arg_self.l))*sizeof(double));
	char** temp_movie = (char**)malloc((arg_self.r - arg_self.l)*sizeof(char*));

	int lptr = arg_self.l;
	int rptr = mid;
	for(int i = 0; i < (arg_self.r - arg_self.l); i++){
		if(rptr == arg_self.r){
			temp_score[i] = arg_self.scores[lptr];
			temp_movie[i] = arg_self.movies[lptr];
			lptr++;
		}
		else if(lptr == mid){
			temp_score[i] = arg_self.scores[rptr];
			temp_movie[i] = arg_self.movies[rptr];
			rptr++;
		}
		else if(arg_self.scores[lptr] > arg_self.scores[rptr] || ( arg_self.scores[lptr] == arg_self.scores[rptr] && strcmp(arg_self.movies[lptr], arg_self.movies[rptr]) < 0 ) ){
			temp_score[i] = arg_self.scores[lptr];
			temp_movie[i] = arg_self.movies[lptr];
			lptr++;
		}
		else{
			temp_score[i] = arg_self.scores[rptr];
			temp_movie[i] = arg_self.movies[rptr];
			rptr++;
		}
	}

	int write_ptr = arg_self.l;

	for(int i = 0; i < (arg_self.r - arg_self.l); i++, write_ptr++){
		arg_self.movies[write_ptr] = temp_movie[i];
		arg_self.scores[write_ptr] = temp_score[i];
	}
	pthread_exit(NULL);
}


int pop(){
	front+=1;
	return front;
}

double dot(double* a, double* b){
	double output = 0;
	for(int i = 0; i < NUM_OF_GENRE; i++){
		output += a[i]*b[i];
	}
	return output;
}

void* deal_request(void* arg){
	request* current_req = reqs[*((int*)arg)];
	char file_name[20];
	sprintf(file_name, "%dt.out", current_req->id);
	char* keywords = current_req->keywords;
	int filtered_movie_num = 0;
	FILE *fp = fopen(file_name, "w");

	double* arr_score = (double*)malloc(num_of_movies*sizeof(double));
	char** arr_movie = (char**)malloc(num_of_movies*sizeof(char*));
	char all_movie[] = "*";
	for(int i = 0; i < num_of_movies; i++){
		if(strcmp(keywords, all_movie) == 0 || strstr(movies[i]->title, keywords) != NULL){
			int title_len = strlen(movies[i]->title);
			arr_movie[filtered_movie_num] = (char*)malloc(title_len + 1);
			memcpy(arr_movie[filtered_movie_num], movies[i]->title, title_len);
			arr_score[filtered_movie_num] = dot(movies[i]->profile, current_req->profile);
			filtered_movie_num++;
		}
	}
	//sort
	sort_arg arg_child;
	arg_child.l = 0;
	arg_child.r = filtered_movie_num;
	arg_child.movies = arr_movie;
	arg_child.scores = arr_score;
	arg_child.layer = 0;

	if(filtered_movie_num == 1){
		sort(arr_movie, arr_score, 1);
		for(int i = 0; i < filtered_movie_num; i++){
			fprintf(fp, "%s\n", arr_movie[i]);
		}

		fclose(fp);
		pthread_exit(NULL);
	}

	if(filtered_movie_num < 10){
		arg_child.layer = 3;
	}
	else if(filtered_movie_num < 20){
		arg_child.layer = 2;
	}
	else if(filtered_movie_num < 40){
		arg_child.layer = 1;
	}

	pthread_t tid;

	pthread_create(&tid, NULL, merge_sort, (void*)&arg_child);
	pthread_join(tid, NULL);

	for(int i = 0; i < filtered_movie_num; i++){
		fprintf(fp, "%s\n", arr_movie[i]);
	}

	fclose(fp);
	pthread_exit(NULL);
}

void process_merge_sort(int l, int r, int layer, int fd, char** mv, double* score, int isparent){
	if(layer == 3){
		char** temp = (char**)malloc((r-l)*sizeof(char*));
		for(int i = 0; i < (r - l); i++){
			temp[i] = mv[l + i];
		}
		sort(temp, &score[l], r - l);
		for(int i = 0; i < r - l; i++){
			strcpy(mv[l + i], temp[i]);
		}
		exit(0);
	}
	else{
		int lpid, rpid;
		int mid = (l + r)/2;

		if((lpid = fork()) == 0){
			process_merge_sort(l, mid, layer + 1, fd, mv, score, 0);
			exit(0);
		}

		if((rpid = fork()) == 0){
			process_merge_sort(mid, r, layer + 1, fd, mv, score, 1);
			exit(0);
		}

		wait(lpid);
		wait(rpid);

		double* temp_score = (double*)malloc((r - l)*sizeof(double));
		char** temp_movie = (char**)malloc((r - l)*sizeof(char*));

		int lptr = l;
		int rptr = mid;
		for(int i = 0; i < r - l; i++){
			if(rptr == r){
				temp_score[i] = score[lptr];
				temp_movie[i] = mv[lptr];
				lptr++;
			}
			else if(lptr == mid){
				temp_score[i] = score[rptr];
				temp_movie[i] = mv[rptr];
				rptr++;
			}
			else if(score[lptr] > score[rptr] || ( score[lptr] == score[rptr] && strcmp(mv[lptr], mv[rptr]) < 0) ){
				temp_score[i] = score[lptr];
				temp_movie[i] = mv[lptr];
				lptr++;
			}
			else{
				temp_score[i] = score[rptr];
				temp_movie[i] = mv[rptr];
				rptr++;
			}
		}

		int write_ptr = l;

		for(int i = 0; i < (r - l); i++, write_ptr++){
			mv[write_ptr] = temp_movie[i];
			score[write_ptr] = temp_score[i];
		}

		if(isparent){
			return;
		}
		exit(0);
	}
}

int main(int argc, char *argv[]){

	if(argc != 1){
#ifdef PROCESS
		fprintf(stderr,"usage: ./pserver\n");
#elif defined THREAD
		fprintf(stderr,"usage: ./tserver\n");
#endif
		exit(-1);
	}

	FILE *fp;

	if ((fp = fopen("./data/movies.txt","r")) == NULL){
		ERR_EXIT("fopen");
	}

	initialize(fp);
	assert(fp != NULL);
	fclose(fp);	

	scanf("%u", num_of_reqs);


#ifdef PROCESS
	
	int mmap_fd = open("/dev/zero", O_RDWR);
	
	request* current_req = reqs[0];
	char file_name[20];
	sprintf(file_name, "%dp.out", current_req->id);
	char* keywords = current_req->keywords;
	int filtered_movie_num = 0;
	FILE *fp2 = fopen(file_name, "w");

	double* arr_score = (double*)mmap(NULL, num_of_movies*sizeof(double), PROT_READ | PROT_WRITE, MAP_SHARED, mmap_fd, 0);
	char** arr_movie = (char**)mmap(NULL, num_of_movies*sizeof(char*), PROT_READ | PROT_WRITE, MAP_SHARED, mmap_fd, 0);
	char all_movie[] = "*";
	for(int i = 0; i < num_of_movies; i++){
		if(strcmp(keywords, all_movie) == 0 || strstr(movies[i]->title, keywords) != NULL){
			arr_movie[filtered_movie_num] = (char*)mmap(NULL, MAX_LEN + 1, PROT_READ | PROT_WRITE, MAP_SHARED, mmap_fd, 0);
			memcpy(arr_movie[filtered_movie_num], movies[i]->title, strlen(movies[i]->title) + 1);
			arr_score[filtered_movie_num] = dot(movies[i]->profile, current_req->profile);
			filtered_movie_num++;
		}
	}

	int layer = 0;

	if(filtered_movie_num < 10){
		layer = 3;
	}
	else if(filtered_movie_num < 20){
		layer = 2;
	}
	else if(filtered_movie_num < 40){
		layer = 1;
	}

	process_merge_sort(0, filtered_movie_num, layer, mmap_fd, arr_movie, arr_score, 1);

	for(int i = 0; i < filtered_movie_num; i++){
		fprintf(fp2, "%s\n", arr_movie[i]);
	}
	fclose(fp2);

#elif defined THREAD

	pthread_t thread_id[32];
	int x[32];

	for(unsigned int i = 0; i < num_of_reqs; i++){
		x[i] = i;
		pthread_create(&thread_id[i], NULL, deal_request, (void*)&x[i]);
	}
	pthread_exit(NULL);

#endif

	return 0;
}

/**=======================================
 * You don't need to modify following code *
 * But feel free if needed.                *
 =========================================**/

request* read_request(){
	int id;
	char buf1[MAX_LEN],buf2[MAX_LEN];
	char delim[2] = ",";

	char *keywords;
	char *token, *ref_pts;
	char *ptr;
	double ret,sum;

	scanf("%u %254s %254s",&id,buf1,buf2);
	keywords = malloc(sizeof(char)*strlen(buf1)+1);
	if(keywords == NULL){
		ERR_EXIT("malloc");
	}

	memcpy(keywords, buf1, strlen(buf1));
	keywords[strlen(buf1)] = '\0';

	double* profile = malloc(sizeof(double)*NUM_OF_GENRE);
	if(profile == NULL){
		ERR_EXIT("malloc");
	}
	sum = 0;
	ref_pts = strtok(buf2,delim);
	for(int i = 0;i < NUM_OF_GENRE;i++){
		ret = strtod(ref_pts, &ptr);
		profile[i] = ret;
		sum += ret*ret;
		ref_pts = strtok(NULL,delim);
	}

	// normalize
	sum = sqrt(sum);
	for(int i = 0;i < NUM_OF_GENRE; i++){
		if(sum == 0)
				profile[i] = 0;
		else
				profile[i] /= sum;
	}

	request* r = malloc(sizeof(request));
	if(r == NULL){
		ERR_EXIT("malloc");
	}

	r->id = id;
	r->keywords = keywords;
	r->profile = profile;

	return r;
}

/*=================initialize the dataset=================*/
void initialize(FILE* fp){

	char chunk[MAX_LEN] = {0};
	char *token,*ptr;
	double ret,sum;
	int cnt = 0;

	assert(fp != NULL);

	// first row
	if(fgets(chunk,sizeof(chunk),fp) == NULL){
		ERR_EXIT("fgets");
	}

	memset(movies,0,sizeof(movie_profile*)*MAX_MOVIES);	

	while(fgets(chunk,sizeof(chunk),fp) != NULL){
		
		assert(cnt < MAX_MOVIES);
		chunk[MAX_LEN-1] = '\0';

		const char delim1[2] = " "; 
		const char delim2[2] = "{";
		const char delim3[2] = ",";
		unsigned int movieId;
		movieId = atoi(strtok(chunk,delim1));

		// title
		token = strtok(NULL,delim2);
		char* title = malloc(sizeof(char)*strlen(token)+1);
		if(title == NULL){
			ERR_EXIT("malloc");
		}
		
		// title.strip()
		memcpy(title, token, strlen(token)-1);
	 	title[strlen(token)-1] = '\0';

		// genres
		double* profile = malloc(sizeof(double)*NUM_OF_GENRE);
		if(profile == NULL){
			ERR_EXIT("malloc");
		}

		sum = 0;
		token = strtok(NULL,delim3);
		for(int i = 0; i < NUM_OF_GENRE; i++){
			ret = strtod(token, &ptr);
			profile[i] = ret;
			sum += ret*ret;
			token = strtok(NULL,delim3);
		}

		// normalize
		sum = sqrt(sum);
		for(int i = 0; i < NUM_OF_GENRE; i++){
			if(sum == 0)
				profile[i] = 0;
			else
				profile[i] /= sum;
		}

		movie_profile* m = malloc(sizeof(movie_profile));
		if(m == NULL){
			ERR_EXIT("malloc");
		}

		m->movieId = movieId;
		m->title = title;
		m->profile = profile;

		movies[cnt++] = m;
	}
	num_of_movies = cnt;

	// request
	scanf("%d",&num_of_reqs);
	assert(num_of_reqs <= MAX_REQ);
	for(int i = 0; i < num_of_reqs; i++){
		reqs[i] = read_request();
	}
}
/*========================================================*/