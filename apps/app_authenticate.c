/*
 * Asterisk -- A telephony toolkit for Linux.
 *
 * Execute arbitrary authenticate commands
 * 
 * Copyright (C) 1999, Mark Spencer
 *
 * Mark Spencer <markster@linux-support.net>
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License
 */

#include <asterisk/lock.h>
#include <asterisk/file.h>
#include <asterisk/logger.h>
#include <asterisk/channel.h>
#include <asterisk/pbx.h>
#include <asterisk/module.h>
#include <asterisk/app.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include <pthread.h>


static char *tdesc = "Authentication Application";

static char *app = "Authenticate";

static char *synopsis = "Authenticate a user";

static char *descrip =
"  Authenticate(password[|options]): Requires a user to enter a"
"given password in order to continue execution.  If the\n"
"password begins with the '/' character, it is interpreted as\n"
"a file which contains a list of valid passwords (1 per line).\n"
"an optional set of opions may be provided by concatenating any\n"
"of the following letters:\n"
"     a - Set account code to the password that is entered\n"
"\n"
"Returns 0 if the user enters a valid password within three\n"
"tries, or -1 otherwise (or on hangup).\n";

STANDARD_LOCAL_USER;

LOCAL_USER_DECL;

static int auth_exec(struct ast_channel *chan, void *data)
{
	int res=0;
	int retries;
	struct localuser *u;
	char password[256]="";
	char passwd[256];
	char *opts;
	char *prompt;
	if (!data || !strlen(data)) {
		ast_log(LOG_WARNING, "Authenticate requires an argument(password)\n");
		return -1;
	}
	LOCAL_USER_ADD(u);
	if (chan->_state != AST_STATE_UP) {
		res = ast_answer(chan);
		if (res) {
			LOCAL_USER_REMOVE(u);
			return -1;
		}
	}
	strncpy(password, data, sizeof(password) - 1);
	opts=strchr(password, '|');
	if (opts) {
		*opts = 0;
		opts++;
	} else
		opts = "";
	/* Start asking for password */
	prompt = "agent-pass";
	for (retries = 0; retries < 3; retries++) {
		res = ast_app_getdata(chan, prompt, passwd, sizeof(passwd) - 2, 0);
		if (res < 0)
			break;
		res = 0;
		if (password[0] == '/') {
			/* Compare against a file */
			char tmp[80];
			FILE *f;
			f = fopen(password, "r");
			if (f) {
				char buf[256] = "";
				while(!feof(f)) {
					fgets(buf, sizeof(buf), f);
					if (!feof(f) && strlen(buf)) {
						buf[strlen(buf) - 1] = '\0';
						if (strlen(buf) && !strcmp(passwd, buf))
							break;
					}
				}
				fclose(f);
				if (strlen(buf) && !strcmp(passwd, buf))
					break;
			} else 
				ast_log(LOG_WARNING, "Unable to open file '%s' for authentication: %s\n", password, strerror(errno));
		} else {
			/* Compare against a fixed password */
			if (!strcmp(passwd, password)) 
				break;
		}
		prompt="auth-incorrect";
	}
	if ((retries < 3) && !res) {
		if (strchr(opts, 'a')) 
			ast_cdr_setaccount(chan, passwd);
	} else {
		if (!res)
			res = ast_streamfile(chan, "vm-goodbye", chan->language);
		if (!res)
			res = ast_waitstream(chan, "");
		res = -1;
	}
	LOCAL_USER_REMOVE(u);
	return res;
}

int unload_module(void)
{
	STANDARD_HANGUP_LOCALUSERS;
	return ast_unregister_application(app);
}

int load_module(void)
{
	return ast_register_application(app, auth_exec, synopsis, descrip);
}

char *description(void)
{
	return tdesc;
}

int usecount(void)
{
	int res;
	STANDARD_USECOUNT(res);
	return res;
}

char *key()
{
	return ASTERISK_GPL_KEY;
}
