#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <fcntl.h> 
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <string.h>
#include <getopt.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <json.h>
#include <mosquitto.h>
#include <time.h>

static char jsonEnclosingArray[256] = "retained";
static int mqtt_port=1883;
static char mqtt_host[256];
static char *mqtt_user_name,*mqtt_passwd;

int outputDebug=0;
#include <sys/time.h>

#define ALARM_SECONDS 5
static int run = 1;


extern char *optarg;
extern int optind, opterr, optopt;

char	*strsave(char *s )
{
char	*ret_val = 0;

ret_val = malloc(strlen(s)+1);
if ( 0 != ret_val) strcpy(ret_val,s);
return ret_val;	
}
typedef struct topics {
	struct topics *left,*right;
	char	*topic;
	char	*retained_message;	/* populated when a retain message is received */
	int message_number;	/* greater than zero means the retained message has been received */
	}	TOPICS;

TOPICS *topic_root = 0;

void add_topic(char *s ) {
	TOPICS *p,*q;
	int	cond;
	int	flag;

	if ( 0 == topic_root ) {
		topic_root = calloc(sizeof(TOPICS),1);
		topic_root->topic = strsave(s);
		return;
	}
	p = topic_root;
	for ( ;0 != p; ) {
		cond = strcmp(p->topic,s);
		q = p;
		if ( 0 == cond )	return;	// no reason to re-subscribe
		if ( 0 < cond ) {
			p = q->left;	flag = 1;
		}
		else {
			p = q->right;	flag = -1;
		}
	}
	/* if here then it is a new topic */
	p = calloc(sizeof(TOPICS),1);
	p->topic = strsave(s);
	if ( 1 == flag )
		q->left = p;
	else
		q->right = p;

}
TOPICS *find_topic( char * s ) {
	TOPICS *p,*q;
	int	cond;
	p = topic_root;
	for ( ;0 != p; ) {
		cond = strcmp(p->topic,s);
		q = p;
		if ( 0 == cond )	return	p;
		if ( 0 < cond ) {
			p = q->left;
		}
		else {
			p = q->right;
		}
	}

return	0;
}




uint64_t microtime() {
	struct timeval time;
	gettimeofday(&time, NULL); 
	return ((uint64_t)time.tv_sec * 1000000) + time.tv_usec;
}

static void signal_handler(int signum) {


	if ( SIGALRM == signum ) {
		run = 0;
	} else if ( SIGPIPE == signum ) {
		fprintf(stderr,"\n# Broken pipe.\n");
		fprintf(stderr,"# Terminating.\n");
		exit(101);
	} else if ( SIGUSR1 == signum ) {
		/* clear signal */
		signal(SIGUSR1, SIG_IGN);

		fprintf(stderr,"# SIGUSR1 triggered data_block dump:\n");
		
		/* re-install alarm handler */
		signal(SIGUSR1, signal_handler);
	} else {
		fprintf(stderr,"\n# Caught unexpected signal %d.\n",signum);
		fprintf(stderr,"# Terminating.\n");
		exit(102);
	}

}

void connect_callback(struct mosquitto *mosq, void *obj, int result) {
	if ( 5 == result ) {
		fprintf(stderr,"# --mqtt-user-name and --mqtt-passwd required at this site.\n");
	}
	printf("# connect_callback, rc=%d\n", result);
}

void message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message) {

	/* cancel pending alarm */
	alarm(0);
	/* set an alarm to send a SIGALARM if data not received within alarmSeconds */
 	alarm(ALARM_SECONDS);


	if ( outputDebug ) {
		fprintf(stderr,"got message '%.*s' for topic '%s' retain = %d\n", 
			message->payloadlen, (char*) message->payload, message->topic,message->retain);
	}
	if ( 0 != message->retain )   {
		TOPICS *p = find_topic(message->topic);
		if ( 0 != p ) {
			p->retained_message = strsave(message->payload);
		}
	}

}
void topics_mosquitto_subscribe(TOPICS *p, struct mosquitto *mosq)
{
if ( 0 == p )	return;
topics_mosquitto_subscribe(p->left,mosq);
//mosquitto_subscribe_v5(mosq, NULL, p->topic, 0,MQTT_SUB_OPT_SEND_RETAIN_ALWAYS);
mosquitto_subscribe(mosq, &p->message_number, p->topic,0);
topics_mosquitto_subscribe(p->right,mosq);
}
void output_retained_messages( TOPICS *p, struct json_object *jarray ) {
	if ( 0 == p ){
		return;
	}
	output_retained_messages(p->left,jarray);

	
	struct json_object *jobj = json_object_new_object();
	struct json_object *tmp = json_object_new_object();


	json_object_object_add(tmp,"message", ( 0 != p->retained_message ) ? json_object_new_string(p->retained_message) : NULL);

	json_object_object_add(jobj,p->topic,tmp);
	
	if ( 0 != p->retained_message ) {
		free(p->retained_message);
		p->retained_message = 0;
	}
	json_object_array_add(jarray,jobj);

	output_retained_messages(p->right,jarray);
}

static int startup_mosquitto(void) {
	char clientid[32];
	struct mosquitto *mosq;
	int rc = 0;

	fprintf(stderr,"# mqtt-modbus start-up\n");

	fprintf(stderr,"# installing signal handlers\n");
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGALRM, signal_handler);

	fprintf(stderr,"# initializing mosquitto MQTT library\n");
	mosquitto_lib_init();

	memset(clientid, 0, 24);
	snprintf(clientid, 31, "mqttGetRetained_%d", getpid());
	mosq = mosquitto_new(clientid, true, 0);

	if (mosq) {
		if ( 0 != mosq,mqtt_user_name && 0 != mqtt_passwd ) {
			mosquitto_username_pw_set(mosq,mqtt_user_name,mqtt_passwd);
		}
		mosquitto_connect_callback_set(mosq, connect_callback);
		mosquitto_message_callback_set(mosq, message_callback);

		fprintf(stderr,"# connecting to MQTT server %s:%d\n",mqtt_host,mqtt_port);
		rc = mosquitto_connect(mosq, mqtt_host, mqtt_port, 60);

		topics_mosquitto_subscribe(topic_root,mosq);

		while (run) {
			rc = mosquitto_loop(mosq, -1, 1);
			}
		mosquitto_destroy(mosq);
	}

	fprintf(stderr,"# mosquitto_lib_cleanup()\n");
	mosquitto_lib_cleanup();

	return rc;
}


enum arguments {
	A_mqtt_host = 512,
	A_mqtt_topic,
	A_mqtt_port,
	A_mqtt_user_name,
	A_mqtt_password,
	A_json_enclosing_array,
	A_log_file_prefix,
	A_log_file_suffix,
	A_log_dir,
	A_unitary_log_file,
	A_split_log_file_by_day,
	A_quiet,
	A_verbose,
	A_help,
};
int main(int argc, char **argv) {
	int n;
	int rc;
	char	cwd[256] = {};
	(void) getcwd(cwd,sizeof(cwd));

	/* command line arguments */
	while (1) {
		// int this_option_optind = optind ? optind : 1;
		int option_index = 0;
		static struct option long_options[] = {
			/* normal program */
		        {"json-enclosing-array",             required_argument, 0, A_json_enclosing_array },
		        {"mqtt-host",                        1,                 0, A_mqtt_host },
		        {"mqtt-topic",                       1,                 0, A_mqtt_topic },
		        {"mqtt-port",                        1,                 0, A_mqtt_port },
		        {"mqtt-user-name",                   1,                 0, A_mqtt_user_name },
		        {"mqtt-passwd",                      1,                 0, A_mqtt_password },
			{"quiet",                            no_argument,       0, A_quiet, },
			{"verbose",                          no_argument,       0, A_verbose, },
		        {"help",                             no_argument,       0, A_help, },
			{},
		};

		n = getopt_long(argc, argv, "", long_options, &option_index);

		if (n == -1) {
			break;
		}
		
		switch (n) {
			case A_json_enclosing_array:
				strncpy(jsonEnclosingArray,optarg,sizeof(jsonEnclosingArray)-1);
				jsonEnclosingArray[sizeof(jsonEnclosingArray)-1]='\0';
				break;
			case A_mqtt_host:	
				strncpy(mqtt_host,optarg,sizeof(mqtt_host));
				break;
			case A_mqtt_topic:
				add_topic(optarg);
				break;
			case A_mqtt_port:
				mqtt_port = atoi(optarg);
				break;
			case A_mqtt_user_name:
				mqtt_user_name = strsave(optarg);
				break;
			case A_mqtt_password:
				mqtt_passwd = strsave(optarg);
				break;
			case A_verbose:
				outputDebug=1;
				fprintf(stderr,"# verbose (debugging) output to stderr enabled\n");
				break;
			case A_help:
				fprintf(stdout,"# --json-enclosing-array\tarray name\t\twrap data array\n");
				fprintf(stdout,"# --mqtt-host\t\t\tmqtt-host is required\tREQUIRED\n");
				fprintf(stdout,"# --mqtt-topic\t\t\tmqtt topic \t\tmust be used at least once\n");
				fprintf(stdout,"# --mqtt-port\t\t\tmqtt port\t\tOPTIONAL\n");
				fprintf(stdout,"# --mqtt-user-name\t\tmaybe required\t\tdepending on system\n");
				fprintf(stdout,"# --mqtt-passwd\t\t\tmaybe required\t\tdepending on system\n");
				fprintf(stdout,"#\n");
				fprintf(stdout,"# --help\t\t\tThis help message then exit\n");
				fprintf(stdout,"#\n");
				exit(0);
		}
	}
	 if ( ' ' >=  mqtt_host[0] ) {
               fprintf(stderr, "# --mqtt-host <required>\n");
               exit(EXIT_FAILURE);
	}
	else
	if ( 0 == topic_root ) {
		fprintf(stderr,"# There must be at least one --mqtt-topic\n");
               exit(EXIT_FAILURE);
	}


	/* install signal handler */
	signal(SIGALRM, signal_handler); /* timeout */
	signal(SIGUSR1, signal_handler); /* user signal to do data block debug dump */
	signal(SIGPIPE, signal_handler); /* broken TCP connection */

	rc = startup_mosquitto();
	
	struct json_object *jobj = json_object_new_object();
	struct json_object *jarray = json_object_new_array();
	output_retained_messages(topic_root,jarray);
	json_object_object_add(jobj,jsonEnclosingArray,jarray);
	fputs(json_object_to_json_string_ext(jobj, JSON_C_TO_STRING_PRETTY),stdout);
	json_object_put(jobj);


	return	rc;
}
