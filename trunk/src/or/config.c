/* Copyright 2001,2002,2003 Roger Dingledine, Matej Pfajfar. */
/* See LICENSE for licensing information */
/* $Id$ */

#include "or.h"

/* enumeration of types which option values can take */
#define CONFIG_TYPE_STRING  0
#define CONFIG_TYPE_CHAR    1
#define CONFIG_TYPE_INT     2
#define CONFIG_TYPE_LONG    3
#define CONFIG_TYPE_DOUBLE  4
#define CONFIG_TYPE_BOOL    5
#define CONFIG_TYPE_LINELIST 6

#define CONFIG_LINE_T_MAXLEN 4096

static FILE *config_open(const unsigned char *filename);
static int config_close(FILE *f);
static struct config_line_t *config_get_commandlines(int argc, char **argv);
static struct config_line_t *config_get_lines(FILE *f);
static void config_free_lines(struct config_line_t *front);
static int config_compare(struct config_line_t *c, char *key, int type, void *arg);
static int config_assign(or_options_t *options, struct config_line_t *list);

/* open configuration file for reading */
static FILE *config_open(const unsigned char *filename) {
  tor_assert(filename);
  if (strspn(filename,CONFIG_LEGAL_FILENAME_CHARACTERS) != strlen(filename)) {
    /* filename has illegal letters */
    return NULL;
  }
  return fopen(filename, "r");
}

/* close configuration file */
static int config_close(FILE *f) {
  tor_assert(f);
  return fclose(f);
}

static struct config_line_t *config_get_commandlines(int argc, char **argv) {
  struct config_line_t *new;
  struct config_line_t *front = NULL;
  char *s;
  int i = 1;

  while(i < argc-1) {
    if(!strcmp(argv[i],"-f")) {
//      log(LOG_DEBUG,"Commandline: skipping over -f.");
      i+=2; /* this is the config file option. ignore it. */
      continue;
    }

    new = tor_malloc(sizeof(struct config_line_t));
    s = argv[i];
    while(*s == '-')
      s++;
    new->key = tor_strdup(s);
    new->value = tor_strdup(argv[i+1]);

    log(LOG_DEBUG,"Commandline: parsed keyword '%s', value '%s'",
      new->key, new->value);
    new->next = front;
    front = new;
    i += 2;
  }
  return front;
}

static struct config_line_t *
config_line_prepend(struct config_line_t *front,
                    const char *key,
                    const char *val)
{
  struct config_line_t *newline;
  newline = tor_malloc(sizeof(struct config_line_t));
  newline->key = tor_strdup(key);
  newline->value = tor_strdup(val);
  newline->next = front;
  return newline;
}

/* parse the config file and strdup into key/value strings. Return list,
 * or NULL if parsing the file failed.
 * Warn and ignore mangled lines. */
static struct config_line_t *config_get_lines(FILE *f) {

  struct config_line_t *front = NULL;
  char line[CONFIG_LINE_T_MAXLEN];
  int result;
  char *key, *value;

  while( (result=parse_line_from_file(line,sizeof(line),f,&key,&value)) > 0) {
    front = config_line_prepend(front, key, value);
  }
  if(result < 0)
    return NULL;
  return front;
}

static void config_free_lines(struct config_line_t *front) {
  struct config_line_t *tmp;

  while(front) {
    tmp = front;
    front = tmp->next;

    free(tmp->key);
    free(tmp->value);
    free(tmp);
  }
}

static int config_compare(struct config_line_t *c, char *key, int type, void *arg) {
  int i;

  if(strncasecmp(c->key,key,strlen(c->key)))
    return 0;

  /* it's a match. cast and assign. */
  log_fn(LOG_DEBUG,"Recognized keyword '%s' as %s, using value '%s'.",c->key,key,c->value);

  switch(type) {
    case CONFIG_TYPE_INT:
      *(int *)arg = atoi(c->value);
      break;
    case CONFIG_TYPE_BOOL:
      i = atoi(c->value);
      if (i != 0 && i != 1) {
        log(LOG_WARN, "Boolean keyword '%s' expects 0 or 1", c->key);
        return 0;
      }
      *(int *)arg = i;
      break;
    case CONFIG_TYPE_STRING:
      tor_free(*(char **)arg);
      *(char **)arg = tor_strdup(c->value);
      break;
    case CONFIG_TYPE_DOUBLE:
      *(double *)arg = atof(c->value);
      break;
    case CONFIG_TYPE_LINELIST:
      /* Note: this reverses the order that the lines appear in.  That's
       * just fine, since we build up the list of lines reversed in the
       * first place. */
      *(struct config_line_t**)arg =
        config_line_prepend(*(struct config_line_t**)arg, c->key, c->value);
      break;
  }
  return 1;
}

/* Iterate through list.
 * For each item, convert as appropriate and assign to 'options'.
 * If an item is unrecognized, return -1 immediately,
 * else return 0 for success. */
static int config_assign(or_options_t *options, struct config_line_t *list) {

  while(list) {
    if(

    /* order matters here! abbreviated arguments use the first match. */

    /* string options */
    config_compare(list, "Address",        CONFIG_TYPE_STRING, &options->Address) ||

    config_compare(list, "BandwidthRate",  CONFIG_TYPE_INT, &options->BandwidthRate) ||
    config_compare(list, "BandwidthBurst", CONFIG_TYPE_INT, &options->BandwidthBurst) ||

    config_compare(list, "DebugLogFile",   CONFIG_TYPE_STRING, &options->DebugLogFile) ||
    config_compare(list, "DataDirectory",  CONFIG_TYPE_STRING, &options->DataDirectory) ||
    config_compare(list, "DirPort",        CONFIG_TYPE_INT, &options->DirPort) ||
    config_compare(list, "DirBindAddress", CONFIG_TYPE_STRING, &options->DirBindAddress) ||
    config_compare(list, "DirFetchPostPeriod",CONFIG_TYPE_INT, &options->DirFetchPostPeriod) ||

    config_compare(list, "ExitNodes",      CONFIG_TYPE_STRING, &options->ExitNodes) ||
    config_compare(list, "EntryNodes",     CONFIG_TYPE_STRING, &options->EntryNodes) ||
    config_compare(list, "ExitPolicy",     CONFIG_TYPE_STRING, &options->ExitPolicy) ||
    config_compare(list, "ExcludeNodes",   CONFIG_TYPE_STRING, &options->ExcludeNodes) ||

    config_compare(list, "Group",          CONFIG_TYPE_STRING, &options->Group) ||

    config_compare(list, "IgnoreVersion",  CONFIG_TYPE_BOOL, &options->IgnoreVersion) ||

    config_compare(list, "KeepalivePeriod",CONFIG_TYPE_INT, &options->KeepalivePeriod) ||

    config_compare(list, "LogLevel",       CONFIG_TYPE_STRING, &options->LogLevel) ||
    config_compare(list, "LogFile",        CONFIG_TYPE_STRING, &options->LogFile) ||
    config_compare(list, "LinkPadding",    CONFIG_TYPE_BOOL, &options->LinkPadding) ||

    config_compare(list, "MaxConn",        CONFIG_TYPE_INT, &options->MaxConn) ||
    config_compare(list, "MaxOnionsPending",CONFIG_TYPE_INT, &options->MaxOnionsPending) ||

    config_compare(list, "Nickname",       CONFIG_TYPE_STRING, &options->Nickname) ||
    config_compare(list, "NewCircuitPeriod",CONFIG_TYPE_INT, &options->NewCircuitPeriod) ||
    config_compare(list, "NumCpus",        CONFIG_TYPE_INT, &options->NumCpus) ||

    config_compare(list, "ORPort",         CONFIG_TYPE_INT, &options->ORPort) ||
    config_compare(list, "ORBindAddress",  CONFIG_TYPE_STRING, &options->ORBindAddress) ||

    config_compare(list, "PidFile",        CONFIG_TYPE_STRING, &options->PidFile) ||
    config_compare(list, "PathlenCoinWeight",CONFIG_TYPE_DOUBLE, &options->PathlenCoinWeight) ||

    config_compare(list, "RouterFile",     CONFIG_TYPE_STRING, &options->RouterFile) ||
    config_compare(list, "RunAsDaemon",    CONFIG_TYPE_BOOL, &options->RunAsDaemon) ||
    config_compare(list, "RecommendedVersions",CONFIG_TYPE_STRING, &options->RecommendedVersions) ||
    config_compare(list, "RendNodes",      CONFIG_TYPE_STRING, &options->RendNodes) ||
    config_compare(list, "RendExcludeNodes",CONFIG_TYPE_STRING, &options->RendExcludeNodes) ||

    config_compare(list, "SocksPort",      CONFIG_TYPE_INT, &options->SocksPort) ||
    config_compare(list, "SocksBindAddress",CONFIG_TYPE_STRING,&options->SocksBindAddress) ||

    config_compare(list, "TrafficShaping", CONFIG_TYPE_BOOL, &options->TrafficShaping) ||

    config_compare(list, "User",           CONFIG_TYPE_STRING, &options->User) ||
    config_compare(list, "RunTesting",     CONFIG_TYPE_BOOL, &options->RunTesting) ||
    config_compare(list, "HiddenServiceDir", CONFIG_TYPE_LINELIST, &options->RendConfigLines)||
    config_compare(list, "HiddenServicePort", CONFIG_TYPE_LINELIST, &options->RendConfigLines)||
    config_compare(list, "HiddenServiceNodes", CONFIG_TYPE_LINELIST, &options->RendConfigLines)||
    config_compare(list, "HiddenServiceExcludeNodes", CONFIG_TYPE_LINELIST, &options->RendConfigLines)
    ) {
      /* then we're ok. it matched something. */
    } else {
      log_fn(LOG_WARN,"Unknown keyword '%s'. Failing.",list->key);
      return -1;
    }

    list = list->next;
  }
  return 0;
}

/* XXX are there any other specifiers we want to give so making
 * a several-thousand-byte string is less painful? */
const char default_dirservers_string[] =
"router moria1 18.244.0.188 9001 9021 9031\n"
"platform Tor 0.0.6rc1 on Linux moria.mit.edu i686\n"
"published 2004-04-25 21:54:28\n"
"bandwidth 800000 10000000\n"
"onion-key\n"
"-----BEGIN RSA PUBLIC KEY-----\n"
"MIGJAoGBANoIvHieyHUTzIacbnWOnyTyzGrLOdXqbcjz2GGMxyHEd5K1bO1ZBNHP\n"
"9i5qLQpN5viFk2K2rEGuG8tFgDEzSWZEtBqv3NVfUdiumdERWMBwlaQ0MVK4C+jf\n"
"y5gZ8KI3o9ZictgPS1AQF+Kk932/vIHTuRIUKb4ILTnQilNvID0NAgMBAAE=\n"
"-----END RSA PUBLIC KEY-----\n"
"signing-key\n"
"-----BEGIN RSA PUBLIC KEY-----\n"
"MIGJAoGBAMHa0ZC/jo2Q2DrwKYF/6ZbmZ27PFYG91u4gUzzmZ/VXLpZ8wNzEV3oW\n"
"nt+I61048fBiC1frT1/DZ351n2bLSk9zJbB6jyGZJn0380FPRX3+cXyXS0Gq8Ril\n"
"xkhMQf5XuNFUb8UmYPSOH4WErjvYjKvU+gfjbK/82Jo9SuHpYz+BAgMBAAE=\n"
"-----END RSA PUBLIC KEY-----\n"
"reject 0.0.0.0/255.0.0.0:*\n"
"reject 169.254.0.0/255.255.0.0:*\n"
"reject 127.0.0.0/255.0.0.0:*\n"
"reject 192.168.0.0/255.255.0.0:*\n"
"reject 10.0.0.0/255.0.0.0:*\n"
"reject 172.16.0.0/255.240.0.0:*\n"
"accept *:20-22\n"
"accept *:53\n"
"accept *:79-80\n"
"accept *:110\n"
"accept *:143\n"
"accept *:443\n"
"accept *:873\n"
"accept *:993\n"
"accept *:995\n"
"accept *:1024-65535\n"
"reject *:*\n"
"router-signature\n"
"-----BEGIN SIGNATURE-----\n"
"o1eAoRHDAEAXsnh5wN++vIwrupd+DbAJ2p3wxHDrmqxTpygzxxCnyQyhMfX03ua2\n"
"4iplyNlwyFwzWcw0sk31otlO2HBYXT1V9G0YxGtKMOeOBMHjfGbUjGvEALHzWi4z\n"
"8DXGJp13zgnUyP4ZA6xaGROwcT6oB5e7UlztvvpGxTg=\n"
"-----END SIGNATURE-----\n"
"\n"
"router moria2 18.244.0.188 9002 9022 9032\n"
"platform Tor 0.0.6rc1 on Linux moria.mit.edu i686\n"
"published 2004-04-25 21:54:30\n"
"bandwidth 800000 10000000\n"
"onion-key\n"
"-----BEGIN RSA PUBLIC KEY-----\n"
"MIGJAoGBAM4Cc/npgYC54XrYLC+grVxJp7PDmNO2DRRJOxKttBBtvLpnR1UaueTi\n"
"kyknT5kmlx+ihgZF/jmye//2dDUp2+kK/kSkpRV4xnDLXZmed+sNSQxqmm9TtZQ9\n"
"/hjpxhp5J9HmUTYhntBs+4E4CUKokmrI6oRLoln4SA39AX9QLPcnAgMBAAE=\n"
"-----END RSA PUBLIC KEY-----\n"
"signing-key\n"
"-----BEGIN RSA PUBLIC KEY-----\n"
"MIGJAoGBAOcrht/y5rkaahfX7sMe2qnpqoPibsjTSJaDvsUtaNP/Bq0MgNDGOR48\n"
"rtwfqTRff275Edkp/UYw3G3vSgKCJr76/bqOHCmkiZrnPV1zxNfrK18gNw2Cxre0\n"
"nTA+fD8JQqpPtb8b0SnG9kwy75eS//sRu7TErie2PzGMxrf9LH0LAgMBAAE=\n"
"-----END RSA PUBLIC KEY-----\n"
"reject 0.0.0.0/255.0.0.0:*\n"
"reject 169.254.0.0/255.255.0.0:*\n"
"reject 127.0.0.0/255.0.0.0:*\n"
"reject 192.168.0.0/255.255.0.0:*\n"
"reject 10.0.0.0/255.0.0.0:*\n"
"reject 172.16.0.0/255.240.0.0:*\n"
"accept *:20-22\n"
"accept *:53\n"
"accept *:79-80\n"
"accept *:110\n"
"accept *:143\n"
"accept *:443\n"
"accept *:873\n"
"accept *:993\n"
"accept *:995\n"
"accept *:1024-65535\n"
"reject *:*\n"
"router-signature\n"
"-----BEGIN SIGNATURE-----\n"
"RKROLwP1ExjTZeg6wuN0pzYqed9IJUd5lAe9hp4ritbnmJAgS6qfww6jgx61CfUR\n"
"6SElhOLE7Q77jAdoL45Ji5pn/Y+Q+E+5lJm1E/ed9ha+YsOPaOc7z6GQ7E4mihCL\n"
"gI1vsw92+P1Ty4RHj6fyD9DhbV19nh2Qs+pvGJOS2FY=\n"
"-----END SIGNATURE-----\n"
"\n"
"router tor26 62.116.124.106 9001 9050 9030\n"
"platform Tor 0.0.6 on Linux seppia i686\n"
"published 2004-05-06 21:33:23\n"
"bandwidth 500000 10000000\n"
"onion-key\n"
"-----BEGIN RSA PUBLIC KEY-----\n"
"MIGJAoGBAMEHdDnpj3ik1AF1xe/VqjoguH2DbANifYqXXfempu0fS+tU9FGo6dU/\n"
"fnVHAZwL9Ek9k2rMzumShi1RduK9p035R/Gk+PBBcLfvwYJ/Nat+ZO/L8jn/3bZe\n"
"ieQd9CKj2LjNGKpRNry37vkwMGIOIlegwK+2us8aXJ7sIvlNts0TAgMBAAE=\n"
"-----END RSA PUBLIC KEY-----\n"
"signing-key\n"
"-----BEGIN RSA PUBLIC KEY-----\n"
"MIGJAoGBAMQgV2gXLbXgesWgeAsj8P1Uvm/zibrFXqwDq27lLKNgWGYGX2ax3LyT\n"
"3nzI1Y5oLs4kPKTsMM5ft9aokwf417lKoCRlZc9ptfRbgxDx90c9GtWVmkrmDvCK\n"
"ae59TMoXIiGfZiwWT6KKq5Zm9/Fu2Il3B2vHGkKJYKixmiBJRKp/AgMBAAE=\n"
"-----END RSA PUBLIC KEY-----\n"
"accept 62.245.184.24:25\n"
"accept 62.116.124.106:6666-6670\n"
"accept *:48099\n"
"reject *:*\n"
"router-signature\n"
"-----BEGIN SIGNATURE-----\n"
"qh/xRoqfLNFzPaB8VdpbdMAwRyuk5qjx4LeLVQ2pDwTZ55PqmG99+VKUNte2WTTD\n"
"7dZEA7um2rueohGe4nYmvbhJWr20/I0ZxmWDRDvFy0b5nwzDMGvLvDw95Zu/XJQ2\n"
"md32NE3y9VZCfbCN+GlvETX3fdR3Svzcm8Kzesg2/s4=\n"
"-----END SIGNATURE-----\n"
;

int config_assign_default_dirservers(void) {
  if(router_set_routerlist_from_string(default_dirservers_string) < 0) {
    log_fn(LOG_WARN,"Bug: the default dirservers internal string is corrupt.");
    return -1;
  }
  return 0;
}

/* Call this function when they're using the default torrc but
 * we can't find it. For now, just hard-code what comes in the
 * default torrc.
 */
static int config_assign_default(or_options_t *options) {

  /* set them up as a client only */
  options->SocksPort = 9050;

  /* plus give them a dirservers file */
  if(config_assign_default_dirservers() < 0)
    return -1;
  return 0;
}

/* prints the usage of tor. */
static void print_usage(void) {
  printf("tor -f <torrc> [args]\n"
         "See man page for more options. This -h is probably obsolete.\n\n"
         "-b <bandwidth>\t\tbytes/second rate limiting\n"
         "-d <file>\t\tDebug file\n"
//         "-m <max>\t\tMax number of connections\n"
         "-l <level>\t\tLog level\n"
         "-r <file>\t\tList of known routers\n");
  printf("\nClient options:\n"
         "-e \"nick1 nick2 ...\"\t\tExit nodes\n"
         "-s <IP>\t\t\tPort to bind to for Socks\n"
         );
  printf("\nServer options:\n"
         "-n <nick>\t\tNickname of router\n"
         "-o <port>\t\tOR port to bind to\n"
         "-p <file>\t\tPID file\n"
         );
}

static int resolve_my_address(or_options_t *options) {
  struct in_addr in;
  struct hostent *rent;
  char localhostname[256];
  int explicit_ip=1;

  if(!options->Address) { /* then we need to guess our address */
    explicit_ip = 0; /* it's implicit */

    if(gethostname(localhostname,sizeof(localhostname)) < 0) {
      log_fn(LOG_WARN,"Error obtaining local hostname");
      return -1;
    }
#if 0 /* don't worry about complaining, as long as it resolves */
    if(!strchr(localhostname,'.')) {
      log_fn(LOG_WARN,"fqdn '%s' has only one element. Misconfigured machine?",address);
      log_fn(LOG_WARN,"Try setting the Address line in your config file.");
      return -1;
    }
#endif
    options->Address = tor_strdup(localhostname);
    log_fn(LOG_DEBUG,"Guessed local host name as '%s'",options->Address);
  }

  /* now we know options->Address is set. resolve it and keep only the IP */

  if(tor_inet_aton(options->Address, &in) == 0) {
    /* then we have to resolve it */
    explicit_ip = 0;
    rent = (struct hostent *)gethostbyname(options->Address);
    if (!rent) {
      log_fn(LOG_WARN,"Could not resolve Address %s. Failing.", options->Address);
      return -1;
    }
    tor_assert(rent->h_length == 4);
    memcpy(&in.s_addr, rent->h_addr,rent->h_length);
  }
  if(!explicit_ip && is_internal_IP(htonl(in.s_addr))) {
    log_fn(LOG_WARN,"Address '%s' resolves to private IP '%s'. "
           "Please set the Address config option to be the IP you want to use.",
           options->Address, inet_ntoa(in));
    return -1;
  }
  tor_free(options->Address);
  options->Address = tor_strdup(inet_ntoa(in));
  log_fn(LOG_DEBUG,"Resolved Address to %s.", options->Address);
  return 0;
}

static void free_options(or_options_t *options) {
  tor_free(options->LogLevel);
  tor_free(options->LogFile);
  tor_free(options->DebugLogFile);
  tor_free(options->DataDirectory);
  tor_free(options->RouterFile);
  tor_free(options->Nickname);
  tor_free(options->Address);
  tor_free(options->PidFile);
  tor_free(options->ExitNodes);
  tor_free(options->EntryNodes);
  tor_free(options->ExcludeNodes);
  tor_free(options->RendNodes);
  tor_free(options->RendExcludeNodes);
  tor_free(options->ExitPolicy);
  tor_free(options->SocksBindAddress);
  tor_free(options->ORBindAddress);
  tor_free(options->DirBindAddress);
  tor_free(options->RecommendedVersions);
  tor_free(options->User);
  tor_free(options->Group);
  config_free_lines(options->RendConfigLines);
}

static void init_options(or_options_t *options) {
/* give reasonable values for each option. Defaults to zero. */
  memset(options,0,sizeof(or_options_t));
  options->LogLevel = tor_strdup("notice");
  options->ExitNodes = tor_strdup("");
  options->EntryNodes = tor_strdup("");
  options->ExcludeNodes = tor_strdup("");
  options->RendNodes = tor_strdup("");
  options->RendExcludeNodes = tor_strdup("");
  options->ExitPolicy = tor_strdup("");
  options->SocksBindAddress = tor_strdup("127.0.0.1");
  options->ORBindAddress = tor_strdup("0.0.0.0");
  options->DirBindAddress = tor_strdup("0.0.0.0");
  options->RecommendedVersions = NULL;
  options->loglevel = LOG_INFO;
  options->PidFile = NULL; // tor_strdup("tor.pid");
  options->DataDirectory = NULL;
  options->PathlenCoinWeight = 0.3;
  options->MaxConn = 900;
  options->DirFetchPostPeriod = 600;
  options->KeepalivePeriod = 300;
  options->MaxOnionsPending = 100;
  options->NewCircuitPeriod = 30; /* twice a minute */
  options->BandwidthRate = 800000; /* at most 800kB/s total sustained incoming */
  options->BandwidthBurst = 10000000; /* max burst on the token bucket */
  options->NumCpus = 1;
  options->RendConfigLines = NULL;
}

/* return 0 if success, <0 if failure. */
int getconfig(int argc, char **argv, or_options_t *options) {
  struct config_line_t *cl;
  FILE *cf;
  char *fname;
  int i;
  int result = 0;
  static int first_load = 1;
  static char **backup_argv;
  static int backup_argc;
  char *previous_pidfile = NULL;
  int previous_runasdaemon = 0;
  int previous_orport = -1;
  int using_default_torrc;

  if(first_load) { /* first time we're called. save commandline args */
    backup_argv = argv;
    backup_argc = argc;
    first_load = 0;
  } else { /* we're reloading. need to clean up old ones first. */
    argv = backup_argv;
    argc = backup_argc;

    /* record some previous values, so we can fail if they change */
    if(options->PidFile)
      previous_pidfile = tor_strdup(options->PidFile);
    previous_runasdaemon = options->RunAsDaemon;
    previous_orport = options->ORPort;
    free_options(options);
  }
  init_options(options);

  if(argc > 1 && (!strcmp(argv[1], "-h") || !strcmp(argv[1],"--help"))) {
    print_usage();
    exit(0);
  }

  if(argc > 1 && (!strcmp(argv[1],"--version"))) {
    printf("Tor version %s.\n",VERSION);
    exit(0);
  }

/* learn config file name, get config lines, assign them */
  i = 1;
  while(i < argc-1 && strcmp(argv[i],"-f")) {
    i++;
  }
  if(i < argc-1) { /* we found one */
    fname = argv[i+1];
    using_default_torrc = 0;
  } else { /* didn't find one, try CONFDIR */
    fname = CONFDIR "/torrc";
    using_default_torrc = 1;
  }
  log(LOG_DEBUG,"Opening config file '%s'",fname);

  cf = config_open(fname);
  if(!cf) {
    if(using_default_torrc == 1) {
      log(LOG_NOTICE, "Configuration file '%s' not present, using reasonable defaults.",fname);
      if(config_assign_default(options) < 0)
        return -1;
    } else {
      log(LOG_WARN, "Unable to open configuration file '%s'.",fname);
      return -1;
    }
  } else { /* it opened successfully. use it. */
    cl = config_get_lines(cf);
    if(!cl) return -1;
    if(config_assign(options,cl) < 0)
      return -1;
    config_free_lines(cl);
    config_close(cf);
  }

/* go through command-line variables too */
  cl = config_get_commandlines(argc,argv);
  if(config_assign(options,cl) < 0)
    return -1;
  config_free_lines(cl);

/* Validate options */

  /* first check if any of the previous options have changed but aren't allowed to */
  if(previous_pidfile && strcmp(previous_pidfile,options->PidFile)) {
    log_fn(LOG_WARN,"During reload, PidFile changed from %s to %s. Failing.",
           previous_pidfile, options->PidFile);
    return -1;
  }
  tor_free(previous_pidfile);

  if(previous_runasdaemon && !options->RunAsDaemon) {
    log_fn(LOG_WARN,"During reload, change from RunAsDaemon=1 to =0 not allowed. Failing.");
    return -1;
  }

  if(previous_orport == 0 && options->ORPort > 0) {
    log_fn(LOG_WARN,"During reload, change from ORPort=0 to >0 not allowed. Failing.");
    return -1;
  }

  if(options->LogLevel) {
    if(!strcmp(options->LogLevel,"err"))
      options->loglevel = LOG_ERR;
    else if(!strcmp(options->LogLevel,"warn"))
      options->loglevel = LOG_WARN;
    else if(!strcmp(options->LogLevel,"notice"))
      options->loglevel = LOG_NOTICE;
    else if(!strcmp(options->LogLevel,"info"))
      options->loglevel = LOG_INFO;
    else if(!strcmp(options->LogLevel,"debug"))
      options->loglevel = LOG_DEBUG;
    else {
      log(LOG_WARN,"LogLevel must be one of err|warn|notice|info|debug.");
      result = -1;
    }
  }

  if(options->ORPort < 0) {
    log(LOG_WARN,"ORPort option can't be negative.");
    result = -1;
  }

  if(options->ORPort && options->DataDirectory == NULL) {
    log(LOG_WARN,"DataDirectory option required if ORPort is set, but not found.");
    result = -1;
  }

  if (options->ORPort) {
    if (options->Nickname == NULL) {
      log_fn(LOG_WARN,"Nickname required if ORPort is set, but not found.");
      result = -1;
    } else {
      if (strspn(options->Nickname, LEGAL_NICKNAME_CHARACTERS) !=
                 strlen(options->Nickname)) {
        log_fn(LOG_WARN, "Nickname '%s' contains illegal characters.", options->Nickname);
        result = -1;
      }
      if (strlen(options->Nickname) > MAX_NICKNAME_LEN) {
        log_fn(LOG_WARN, "Nickname '%s' has more than %d characters.",
               options->Nickname, MAX_NICKNAME_LEN);
        result = -1;
      }
    }
  }

  if(options->ORPort) { /* get an IP for ourselves */
    if(resolve_my_address(options) < 0)
      result = -1;
  }

  if(options->SocksPort < 0) {
    log(LOG_WARN,"SocksPort option can't be negative.");
    result = -1;
  }

  if(options->SocksPort == 0 && options->ORPort == 0) {
    log(LOG_WARN,"SocksPort and ORPort are both undefined? Quitting.");
    result = -1;
  }

  if(options->DirPort < 0) {
    log(LOG_WARN,"DirPort option can't be negative.");
    result = -1;
  }

  if(options->DirPort && options->RecommendedVersions == NULL) {
    log(LOG_WARN,"Directory servers must configure RecommendedVersions.");
    result = -1;
  }

  if(options->SocksPort > 1 &&
     (options->PathlenCoinWeight < 0.0 || options->PathlenCoinWeight >= 1.0)) {
    log(LOG_WARN,"PathlenCoinWeight option must be >=0.0 and <1.0.");
    result = -1;
  }

  if(options->MaxConn < 1) {
    log(LOG_WARN,"MaxConn option must be a non-zero positive integer.");
    result = -1;
  }

  if(options->MaxConn >= MAXCONNECTIONS) {
    log(LOG_WARN,"MaxConn option must be less than %d.", MAXCONNECTIONS);
    result = -1;
  }

  if(options->DirFetchPostPeriod < 1) {
    log(LOG_WARN,"DirFetchPostPeriod option must be positive.");
    result = -1;
  }
  if(options->DirFetchPostPeriod > MIN_ONION_KEY_LIFETIME/2) {
    log(LOG_WARN,"DirFetchPostPeriod is too large; clipping.");
    options->DirFetchPostPeriod = MIN_ONION_KEY_LIFETIME/2;
  }

  if(options->KeepalivePeriod < 1) {
    log(LOG_WARN,"KeepalivePeriod option must be positive.");
    result = -1;
  }

  /* XXX look at the various nicknamelists and make sure they're
   * valid and don't have hostnames that are too long.
   */

  if (rend_config_services(options) < 0) {
    result = -1;
  }

  return result;
}

/*
  Local Variables:
  mode:c
  indent-tabs-mode:nil
  c-basic-offset:2
  End:
*/
