
1. Preface.

Tcl is easy yet powerful scripting language. climm provides Tcl support
both for evaluating Tcl constructions from climm command line and
writing Tcl "hooks" to handle different events (incoming messages,
status changes, etc). This short HOWTO provides an introduction to
writing Tcl scripts for climm. I assume the reader has basic knowledge
of Tcl itself - just like I have. ;)

Why this HOWTO? When trying to implement a simple hook I discovered
that the documentation for this climm functionality is rather sparse
and then I had to study climm sources to get my task done. As not
everybody can read C source code freely (well, at least I cannot) I
decided to try to cover the theme. Hope my efforts will help somebody.

Ah, and of course comments and additions are welcome. Reach me at
jeff < at > macloue.com.

2. Prerequisites - compiling climm with Tcl support.

If your system contains a usable Tcl 8.4 installation climm build
system will detect it and use automatically. However you can disable
Tcl support with --disable-tcl switch - this may give your build a
small performance boost.

3. Basics.

When climm is compiled with Tcl support it creates an internal Tcl
interpreter upon startup. The user can use this interpreter to execute
tcl code by using one of the following commands at climm promt:

	tcl <command>

Executes inline Tcl <command> in climm Tcl interpreter.

Arbitrary Tcl commands can be used, for example:

climm> tcl puts [expr 1+2]
3
climm>

Unlike tclsh climm doesn't automatically echo the result of evaluation
so the following command prints nothing:

climm> tcl expr 1+2
climm>

So remember to "puts" the result of your calculations!

	tclscript <file>

Executes <file> as a Tcl script in scope of climm Tcl interpreter.

Supposing that the following script is stored in $HOME/.climm/hello.tcl:

puts "Hello, world"

the following command will provide:

climm> tclscript .climm/hello.tcl
Hello, world
climm>

You can also execute Tcl script upon climm startup by adding option
"tclscript" to "[General]" section of your climmrc file. In this case
script path can be given relative to climm base directory (usually
$HOME/.climm).

4. Tcl extensions provided by climm.

As most application do, climm provides some extensions to common Tcl
syntax. The extension command is "climm" and you can get a short usage
description by "climm help" ("tcl climm help" or even "tcl help" from
the climm prompt). I will describe "climm" subcommands in slightly more
depth below, following Tcl man pages syntax.

	climm exec <cmd>

Executes <cmd> as if it is given at climm command prompt. So, the
following code:

climm exec "msg Commander Good day, commander!"

will send message "Good day, commander!" to Commander contact.

	climm nick <uin>

Finds contact name for <uin>. For example, this code:

climm nick 48130434

will return my nickname in your contact list (or empty string if I'm
not there).

The following commands deal with hooks that can be installed into
climm. I will describe them later.

	climm receive <script> ?<contact>?

Installs message hook <script>, invoked on every incoming message or if
<contact> is given - on incoming messages from <contact>.

	climm unreceive ?<contact>?

Removes global message hook or message hook for <contact>.

[NB: Actually you need to provide exactly the same arguments as to
"climm receive" command to successfully remove the hook.]

	climm event <script>

Installs an event hook.

	climm unevent

Removes event hook.

[NB: Actually you need to provide exactly the same arguments as to
"climm event" command to successfully remove the hook.]

	climm hooks

Returns list (in Tcl meaning, see list(n)) of hooks installed. The list
consists of two-element lists with first element being a hook type
(either "event hook" for event hook or contact name for receive hooks)
and second showing the script which is executed.

5. Hooks - basic ideas and examples.

When compiled with Tcl support climm can execute different Tcl commands
upon certain events. As said above, "climm receive" command can be used
to make given <script> to be executed upon incoming message arrival.
Actually as far as I can guess from climm source code climm just
executes the following construct:

	<script> <uin> <message>

when message <message> is received from UIN <uin>. This requires
<script> to be no more than a name of procedure accepting two
parameters - or a construction like that:

puts "some string"; return;

This will make the code executed as follows:

puts "some string"; return; 48130434 {Hello!}

and hook arguments are ignored - simple but ugly, isn't it? And not as
flexible as a procedure too - isn't it useful to know whom we received
a message from?

But enough chit-chat, let's try to write some hooks already!

5.1. Message hooks

Message hooks are simple. Unlike event hooks there are always two
arguments to the script. Let's write a simple procedure and attach it
as a message hook.

proc msghook {uin message} {puts "Oh, message from $uin!"}
climm receive msghook

In climm prompt this will look as follows:

climm> tcl proc msghook {uin message} {puts "Oh, message from $uin!"}
climm> tcl climm receive msghook

Let's try to test it:

climm> msg 48130434 test
14:39:13              48130434 {<< test
Oh, message from 48130434!
14:39:14              48130434 >>} test

It works, doesn't it? Then let's try to remove the hook:

climm> tcl climm unreceive msghook

The hook is now removed.

This hook was global, let's make it filter UINs.

climm> tcl climm receive msghook Larry_Laffer
climm> msg 48130434 test
14:42:35              48130434 {<< test
14:42:35              48130434 >>} test

The hook is not executed. To remove it we need to give the following
command:

climm> tcl climm unreceive msghook Larry_Laffer

5.2. Event hooks

Event hooks are complex. At the moment of writing climm doesn't
distinguish different event types - there is only one global event hook
and it is called with variable number of parameters. The first is
always an event type - so it's up to hook procedure to decide whether
to continue processing or return. Second is always UIN of the contact
who triggered the event. Other parameters can vary so precautions need
to be made not to receive many error messages.

5.2.1. Event: status change.

Let's try to implement simple "status change" event handler. Status
change event happens every time a contact changes its status
(naturally) and executes the following Tcl code:

	<event handler script> status <uin> <status>

So, let's write something like this:

proc evhandler {type uin args} {
	if { $type != "status" } then return
	set status [lindex $args 0]
	set nick [climm nick $uin]
	puts "$nick changes status to $status"
}

This procedure analyzes event type and if it is a status change event
extracts status value and prints a message. Let's attach it as an event
handler:

climm event evhandler

If the procedure is stored in file $HOME/.climm/evhandler.tcl the
following commands will do the trick:

climm> tclscript .climm/evhandler.tcl
climm> tcl climm event evhandler

Then, when a contact changes his/her status, we will get the following:

15:17:46               Contact1 changed status to occupied.
Contact1 changes status to occupied

Or even:

15:19:39              Contact2 logged off.
Contact2 changes status to logged_off

Going offline is also a status change.

6. Hooks reference.

6.1. Message hook.

Install syntax:

	climm receive <script> ?<contact>?

Remove syntax:

	climm unreceive <script> ?<contact>?

Tcl code executed upon message arrival (from <contact> if specified):

	<script> <uin> <message>

6.2. Event hooks.

Install syntax:

	climm event <script>

Remove syntax:

	climm unevent <script>

Tcl code executed upon event:

	<script> <type> <uin> ?<arg1>? ...

List of optional arguments depends on event <type>.

6.2.1. Event: status change.

When a contact <uin> changes his status to <status> the following Tcl
code is executed:

	<script> status <uin> <status>

6.2.2. Event: message received.

This event is triggered when incoming message arrives. Although it's
better to use message hooks to handle events of this type you can
handle messages through the event hook as well - it may useful in some
cases because message and event hooks are invoked independently.

When a message <message> is received from user <uin> the following Tcl
code executed is as follows:

	<script> message <uin> <message>

6.2.3. Event: incoming file request.

This event is triggered when file transfer request arrives. The
following Tcl code is executed:

	<script> file_request <uin> <file name> <bytes> <ref>

FIXME: I don't know much about ICQ file transfers, so can somebody tell
me what the <ref> is?

6.2.4. Event: added to contact list.

This event is triggered when you are added to somebody's contact list.
When user <uin> adds you to his contact list the following Tcl code is
executed:

	<script> contactlistadded <uin>

6.2.5. Event: authorization.

Events of this group are triggered when authorization messages arrive.
When auth message arrives from user <uin> the following Tcl code is
executed:

	<script> authorization <uin> <authtype>

<authtype> can be:

 - request - when incoming auth request is received from user <uin>;
 - refused - if your auth request is refused by user <uin>;
 - granted - if your auth request is granted by user <uin>.

6.2.6. Event: Web message arrived.

This event is triggered when you receive a message from ICQ web site
("web message"). The following Tcl code is executed:

	<script> web <uin> <t0> <t3> <t4> <t5>

FIXME: add t0-t5 parameters description; not sure what <uin> is in this
case.

6.2.7. Event: Mail message arrived.

This event is triggered when you receive a ICQ Mail message. The
following Tcl code is executed:

	<script> mail <uin> <t0> <t3> <t4> <t5>

FIXME: add t0-t5 parameters description; not sure what <uin> is in this case

6.2.8. Event: SSL.

Events of this group are triggered at various stage of SSL connection setup.

When it is detected that the user <uin>'s client doesn't support SSL
channel the following Tcl code is executed:

	<script> ssl <uin> no_candidate

When it is detected that the user <uin>'s client supports SSL channel
the following Tcl code is executed:

	<script> ssl <uin> candidate

When SSL connection is tried to user <uin>, the following Tcl code is
executed if remote hasn't SSL connetion enabled:

	<script> ssl <uin> failed precondition

When SSL connection is tried to user <uin>, the following Tcl code is
executed if SSL connection initialization fails:

	<script> ssl <uin> failed init

When SSL connection is tried to user <uin>, the following Tcl code is
executed if SSL key exchange fails:

	<script> ssl <uin> failed key

When SSL connection is tried to user <uin>, the following Tcl code is
executed if SSL handshake fails:

	<script> ssl <uin> failed handshake

And, finally, when SSL connection to user <uin> is successfully
established the following Tcl code is executed:

	<script> ssl <uin> ok

If the SSL connection to <uin> fails at some stage finally the
following Tcl code is executed:

	<script> ssl <uin> incapable
