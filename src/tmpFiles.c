/*
 * tmpFiles.c
 *
 *  Created on: Aug 4, 2023
 *      Author: wael
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "getdiff.h"
#include "util.h"  /* includes list.h **/
#include "ztError.h"

int writeScript(char *fileName){

  /* we depend on this script, I just could not risk leaving it in its own file */

  FILE   *filePtr;
  int    result;

  char const *script =
    "#! /usr/bin/env python3\n"
    "\n"
    "import argparse\n"
    "import json\n"
    "import re\n"
    "import requests\n"
    "import sys\n"
    "from getpass import getpass\n"
    "\n"
    "CUSTOM_HEADER = {\"user-agent\": \"oauth_cookie_client.py\"}\n"
    "\n"
    "def report_error(message):\n"
    "    sys.stderr.write(\"{}\\n\".format(message))\n"
    "    exit(1)\n"
    "\n"
    "\n"
    "def find_authenticity_token(response):\n"
    "    \"\"\"\n"
    "    Search the authenticity_token in the response of the server\n"
    "    \"\"\"\n"
    "    pattern = r\"name=\\\"csrf-token\\\" content=\\\"([^\\\"]+)\\\"\"\n"
    "    m = re.search(pattern, response)\n"
    "    if m is None:\n"
    "        report_error(\"Could not find the authenticity_token in the website to be scraped.\")\n"
    "    try:\n"
    "        return m.group(1)\n"
    "    except IndexError:\n"
    "        sys.stderr.write(\"ERROR: The login form does not contain an authenticity_token.\\n\")\n"
    "        exit(1)\n"
    "\n"
    "\n"
    "parser = argparse.ArgumentParser(description=\"Get a cookie to access service protected by OpenStreetMap OAuth 1.0a and osm-internal-oauth\")\n"
    "parser.add_argument(\"-o\", \"--output\", default=None, help=\"write the cookie to the specified file instead to STDOUT\", type=argparse.FileType(\"w+\"))\n"
    "parser.add_argument(\"-u\", \"--user\", default=None, help=\"user name\", type=str)\n"
    "parser.add_argument(\"-p\", \"--password\", default=None, help=\"Password, leave empty to force input from STDIN.\", type=str)\n"
    "parser.add_argument(\"-s\", \"--settings\", default=None, help=\"JSON file containing parameters\", type=argparse.FileType(\"r\"))\n"
    "parser.add_argument(\"-c\", \"--consumer-url\", default=None, help=\"URL of the OAuth cookie generation API of the provider who provides you OAuth protected access to their ressources\", type=str)\n"
    "parser.add_argument(\"-f\", \"--format\", default=\"http\", help=\"Output format: 'http' for the value of the HTTP 'Cookie' header or 'netscape' for a Netscape-like cookie jar file\", type=str, choices=[\"http\", \"netscape\"])\n"
    "parser.add_argument(\"--osm-host\", default=\"https://www.openstreetmap.org/\", help=\"hostname of the OSM API/website to use (e.g. 'www.openstreetmap.org' or 'master.apis.dev.openstreetmap.org')\", type=str)\n"
    "\n"
    "\n"
    "args = parser.parse_args()\n"
    "settings = {}\n"
    "if args.settings is not None:\n"
    "    settings = json.load(args.settings)\n"
    "\n"
    "username = settings.get(\"user\", args.user)\n"
    "if username is None:\n"
    "    username = input(\"Please enter your user name and press ENTER: \")\n"
    "if username is None:\n"
    "    report_error(\"The username must not be empty.\")\n"
    "password = settings.get(\"password\", args.password)\n"
    "if password is None:\n"
    "    password = getpass(\"Please enter your password and press ENTER: \")\n"
    "if len(password) == 0:\n"
    "    report_error(\"The password must not be empty.\")\n"
    "\n"
    "osm_host = settings.get(\"osm_host\", args.osm_host)\n"
    "consumer_url = settings.get(\"consumer_url\", args.consumer_url)\n"
    "if consumer_url is None:\n"
    "    report_error(\"No consumer URL provided\")\n"
    "\n"
    "# get request token\n"
    "url = consumer_url + \"?action=request_token\"\n"
    "r = requests.post(url, data={}, headers=CUSTOM_HEADER)\n"
    "if r.status_code != 200:\n"
    "    report_error(\"POST {}, received HTTP code {} but expected 200\".format(url, r.status_code))\n"
    "# NOTE: removed the word 'status' from the above line. W.H.\n"
    "json_response = json.loads(r.text)\n"
    "authorize_url = osm_host + \"/oauth/authorize\"\n"
    "try:\n"
    "    oauth_token = json_response[\"oauth_token\"]\n"
    "    oauth_token_secret_encr = json_response[\"oauth_token_secret_encr\"]\n"
    "except KeyError:\n"
    "    report_error(\"oauth_token was not found in the first response by the consumer\")\n"
    "\n"
    "# get OSM session\n"
    "login_url = osm_host + \"/login?cookie_test=true\"\n"
    "s = requests.Session()\n"
    "r = s.get(login_url, headers=CUSTOM_HEADER)\n"
    "if r.status_code != 200:\n"
    "    report_error(\"GET {}, received HTTP code {}\".format(login_url, r.status_code))\n"
    "\n"
    "# login\n"
    "authenticity_token = find_authenticity_token(r.text)\n"
    "login_url = osm_host + \"/login\"\n"
    "r = s.post(login_url, data={\"username\": username, \"password\": password, \"referer\": \"/\", \"commit\": \"Login\", \"authenticity_token\": authenticity_token}, allow_redirects=False, headers=CUSTOM_HEADER)\n"
    "if r.status_code != 302:\n"
    "    report_error(\"POST {}, received HTTP code {} but expected 302\".format(login_url, r.status_code))\n"
    "\n"
    "# authorize\n"
    "authorize_url = \"{}/oauth/authorize?oauth_token={}\".format(osm_host, oauth_token)\n"
    "r = s.get(authorize_url, headers=CUSTOM_HEADER)\n"
    "if r.status_code != 200:\n"
    "    report_error(\"GET {}, received HTTP code {} but expected 200\".format(authorize_url, r.status_code))\n"
    "authenticity_token = find_authenticity_token(r.text)\n"
    "\n"
    "post_data = {\"oauth_token\": oauth_token, \"oauth_callback\": \"\", \"authenticity_token\": authenticity_token, \"allow_read_prefs\": [0, 1], \"commit\": \"Save changes\"}\n"
    "authorize_url = \"{}/oauth/authorize\".format(osm_host)\n"
    "r = s.post(authorize_url, data=post_data, headers=CUSTOM_HEADER)\n"
    "if r.status_code != 200:\n"
    "    report_error(\"POST {}, received HTTP code {} but expected 200\".format(authorize_url, r.status_code))\n"
    "\n"
    "# logout\n"
    "logout_url = \"{}/logout\".format(osm_host)\n"
    "r = s.get(logout_url, headers=CUSTOM_HEADER)\n"
    "if r.status_code != 200 and r.status_code != 302:\n"
    "    report_error(\"POST {}, received HTTP code {} but expected 200 or 302\".format(logout_url))\n"
    "\n"
    "# get final cookie\n"
    "url = consumer_url + \"?action=get_access_token_cookie&format={}\".format(args.format)\n"
    "r = requests.post(url, data={\"oauth_token\": oauth_token, \"oauth_token_secret_encr\": oauth_token_secret_encr}, headers=CUSTOM_HEADER)\n"
    "\n"
    "cookie_text = r.text\n"
    "if not cookie_text.endswith(\"\\n\"):\n"
    "    cookie_text += \"\\n\"\n"
    "\n"
    "if not args.output:\n"
    "    sys.stdout.write(cookie_text)\n"
    "else:\n"
    "    args.output.write(cookie_text)\n"; /* end script **/


  ASSERTARGS(fileName);

  errno = 0;

  /* we assume fileName is good filename - was checked by caller. **/
  filePtr = fopen ( fileName, "w");
  if ( ! filePtr ){
    fprintf (stderr, "%s: Error could not open file for writing! <%s>\n",
             progName, fileName);
    fprintf(stderr, "System error message: %s\n\n", strerror(errno));
    return ztOpenFileError;
  }

  /* write it **/
  result = fprintf (filePtr, "%s", script);

  /* check result **/
  if (result != strlen(script)){

    if (result < 0){
      fprintf (stderr, "%s: Output error in fprintf() call in writeScript() function.\n", progName);
      return ztFailedLibCall;
    }

    fprintf (stderr, "%s: writeScript(): Error returned from fprintf() to script file\n"
	     "result does not match script length!\n", progName);
    return ztWriteError;
  }

  fflush(filePtr);
  fclose(filePtr);

  return ztSuccess;

} /* END writeScript() **/

int writeJSONfile(char *filename, char *usr, char *pswd){

  FILE  *filePtr;
  char  *template =
    "{\n"
    "  \"user\": \"%s\",\n"
    "  \"password\": \"%s\",\n"
    "  \"osm_host\": \"https://www.openstreetmap.org\",\n"
    "  \"consumer_url\": \"https://osm-internal.download.geofabrik.de/get_cookie\"\n"
    "}\n";

  ASSERTARGS (filename && usr && pswd);

  errno = 0;

  /* filename is assumed to be checked by caller and is good name **/
  filePtr = fopen ( filename, "w");
  if ( filePtr == NULL){
    fprintf (stderr, "%s: Error could not open file for writing! <%s>\n",
             progName, filename);
    fprintf(stderr, "System error message: %s\n\n", strerror(errno));
    return ztOpenFileError;
  }

  fprintf (filePtr, template, usr, pswd);

  fflush (filePtr);
  fclose(filePtr);

  return ztSuccess;

} /* END writeJSONfile() **/

