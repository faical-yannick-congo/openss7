/*
 @(#) src/java/javax/sip/header/AllowHeader.java <p>
 
 Copyright &copy; 2008-2015  Monavacon Limited <a href="http://www.monavacon.com/">&lt;http://www.monavacon.com/&gt;</a>. <br>
 Copyright &copy; 2001-2008  OpenSS7 Corporation <a href="http://www.openss7.com/">&lt;http://www.openss7.com/&gt;</a>. <br>
 Copyright &copy; 1997-2001  Brian F. G. Bidulock <a href="mailto:bidulock@openss7.org">&lt;bidulock@openss7.org&gt;</a>. <p>
 
 All Rights Reserved. <p>
 
 This program is free software: you can redistribute it and/or modify it under
 the terms of the GNU Affero General Public License as published by the Free
 Software Foundation, version 3 of the license. <p>
 
 This program is distributed in the hope that it will be useful, but <b>WITHOUT
 ANY WARRANTY</b>; without even the implied warranty of <b>MERCHANTABILITY</b>
 or <b>FITNESS FOR A PARTICULAR PURPOSE</b>.  See the GNU Affero General Public
 License for more details. <p>
 
 You should have received a copy of the GNU Affero General Public License along
 with this program.  If not, see
 <a href="http://www.gnu.org/licenses/">&lt;http://www.gnu.org/licenses/&gt</a>,
 or write to the Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA
 02139, USA. <p>
 
 <em>U.S. GOVERNMENT RESTRICTED RIGHTS</em>.  If you are licensing this
 Software on behalf of the U.S. Government ("Government"), the following
 provisions apply to you.  If the Software is supplied by the Department of
 Defense ("DoD"), it is classified as "Commercial Computer Software" under
 paragraph 252.227-7014 of the DoD Supplement to the Federal Acquisition
 Regulations ("DFARS") (or any successor regulations) and the Government is
 acquiring only the license rights granted herein (the license rights
 customarily provided to non-Government users).  If the Software is supplied to
 any unit or agency of the Government other than DoD, it is classified as
 "Restricted Computer Software" and the Government's rights in the Software are
 defined in paragraph 52.227-19 of the Federal Acquisition Regulations ("FAR")
 (or any successor regulations) or, in the cases of NASA, in paragraph
 18.52.227-86 of the NASA Supplement to the FAR (or any successor regulations). <p>
 
 Commercial licensing and support of this software is available from OpenSS7
 Corporation at a fee.  See
 <a href="http://www.openss7.com/">http://www.openss7.com/</a>
 */

package javax.sip.header;

/**
    The Allow header field lists the set of methods supported by the User Agent generating the
    message. All methods, including ACK and CANCEL, understood by the User Agent MUST be included in
    the list of methods in the Allow header field, when present. The absence of an Allow header
    field MUST NOT be interpreted to mean that the User Agent sending the message supports no
    methods. Rather, it implies that the User Agent is not providing any information on what methods
    it supports. Supplying an Allow header field in responses to methods other than OPTIONS reduces
    the number of messages needed. <p> <dt>For Example:<br><code>Allow: INVITE, ACK, OPTIONS,
    CANCEL, BYE</code>
    @version 1.2.2
    @author Monavacon Limited
  */
public interface AllowHeader extends Header {
    /** Name of AllowHeader. */
    static final java.lang.String NAME = "Allow";
    /**
        Sets the Allow header value. The argument may be a single method name (eg "ACK") or a comma
        delimited list of method names (eg "ACK, CANCEL, INVITE").
        @param method The java.lang.String defining the method supported in this AllowHeader.
        @exception java.text.ParseException Thrown when an error was encountered while parsing the
        method supported.
      */
    void setMethod(java.lang.String method) throws java.text.ParseException;
    /**
        Gets the method of the AllowHeader. Returns null if no method is defined in this Allow
        Header.
        @return The string identifing the method of AllowHeader.
      */
    java.lang.String getMethod();
}

// vim: sw=4 et tw=72 com=srO\:/**,mb\:*,ex\:*/,srO\:/*,mb\:*,ex\:*/,b\:TRANS,\://,b\:#,\:%,\:XCOMM,n\:>,fb\:-
