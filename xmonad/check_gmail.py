#!/usr/bin/env python

#========================================================================
#      Copyright 2007 Raja <rajajs@gmail.com>
#
#      This program is free software; you can redistribute it and/or modify
#      it under the terms of the GNU General Public License as published by
#      the Free Software Foundation; either version 2 of the License, or
#      (at your option) any later version.
#==========================================================================

# ======================================================================
# Modified from code originally written by Baishampayan Ghose
# Copyright (C) 2006 Baishampayan Ghose <b.ghose@ubuntu.com>
# ======================================================================

import sys
import urllib
import feedparser

_url = "https://mail.google.com/gmail/feed/atom"

##################   Edit here      #######################

_pwdfile = '/home/rlblaster/.xmonad/.pwd'  # pwd stored in a file
_username = 'rlblaster'

return_value = 1

###########################################################

class GmailRSSOpener(urllib.FancyURLopener):
    '''Logs on with stored password and username
       Password is stored in a hidden file in the home folder'''

    def prompt_user_passwd(self, host, realm):
        #uncomment line below if password directly entered in script.
        pwd = open(_pwdfile).read().strip()
        return (_username, pwd)

def auth():
    '''The method to do HTTPBasicAuthentication'''
    opener = GmailRSSOpener()
    f = opener.open(_url)
    feed = f.read()
    return feed

def showmail(feed):
    '''Parse the Atom feed and print a summary'''
    global return_value

    atom = feedparser.parse(feed)
    newmails = len(atom.entries)
    if newmails == 0:
        return_value = 1
    else:
        return_value = 0

if __name__ == "__main__":
    try:
        feed = auth()
        showmail(feed)
    except:
        pass
    sys.exit(return_value)
