/*
 * pagetempl.c.in
 * 		templates for generated html pages
 */

const char template_pagehead[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "\n"
    "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\"\n"
    " \"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\">\n"
    "\n"
    "<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\">\n"
    "    <head>\n"
    "        <meta http-equiv=\"Content-Type\" content=\"application/xhtml+xml; charset=utf-8\" />\n"
    "\n"
    "        <meta name=\"generator\" content=\"dissemina\" />\n"
    "\n"
    "        <title>%s</title>\n"
    "    </head>\n"
;

const char template_bodystart[] = "	<body>\n"
    "\n"
;

const char template_bodyend[] = "    </body>\n"
    "</html>\n"
    "\n"
;

