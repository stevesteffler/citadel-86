Error Messages for Citadel-86
V3.32
90Jul08

   This is an incomplete listing of error messages, both fatal and warning,
Citadel-86 can generate, along with possible explanations and even plans
for fixing same.  These can occur both in CONFG and CTDL.

FATAL MESSAGES:
   These messages will usually show up at the console and in a file named
CRASH.

------------

"INTERNAL: No room for new option!"

   PROBLEM: This error indicates you have either a trashed CTDLPROT.SYS file
or have defined either too many external upload or download protocols.  The
limit is 15 for both modes.

   SOLUTIONS: It should be obvious if the problem is in uploads or downloads.
Remember, you're only allowed about 20 external protocols in each of uploads
and downloads (not total, but per).  Check to make sure your CTDLPROT.SYS is
correct (not trashed up) -- if Ease thinks it's ok, then check it by hand.
You'll find CTDLPROT.SYS in your #ROOMAREA directory.
 
------------

"?getLog-read fail//EOF detected (1)!"
"?getLog-read fail//EOF detected (2)!"
"?getLog-read fail//EOF detected (3)!"

   PROBLEM: An attempt to read an account from the log failed.  1 indicates
the failure occurred while reading the main body of the account, 2 the failure
occurred while trying to read the room pointers, 3 the failure occurred in
reading the Mail> data.  This is a serious problem.

   SOLUTIONS? If the cause isn't a damaged CTDLLOG.SYS which will require you
to erase it and reconfigure (and thus lose all of your accounts if you haven't
backed up), then there are a few other causes:

 o Changing #MAIL-SLOTS in CTDLCNFG.SYS without running one of the data
expansion contraction utilities (Ease is preferred, but the obsolete DataChng
also will do the job).

 o Changing #MAXROOMS in CTDLCNFG.SYS without etc.

 o Changing both #MAXROOMS and #MAIL-SLOTS in the same session with Ease.
While this should be theoretically possible, it is NOT recommended.

 o Internal bug.  Report it if the other causes seem improbable.

------------

"?putLog-write fail (1)!"
"?putLog-write fail (2)!"
"?putLog-write fail (3)!"

   PROBLEM: As you might guess, these are related to those above.  They indicate
an attempt to update the log failed.  1 indicates the failure occurred
while writing the main body of the account, 2 while writing the room pointers,
and 3 the Mail> data.  This problem is very serious.  The most probable
cause is some sort of odd mixup in MS-DOS, or an internal bug.

------------

"?getRoom(): read failed//error or EOF (1)!"
"?getRoom(): read failed//error or EOF (2)!"

   PROBLEM: An attempt to read a room record failed.  1 indicates the failure
came while reading the main body of the room, 2 the messages themselves.

   SOLUTIONS?  This is a serious problem.  It can be caused by changing
#MAXROOMS without employing Ease or DataChng.  Otherwise, you may have found a
bug.

------------

"?putRoom(): fseek failure!"
"?putRoom() crash!//0 returned!!!(1)"
"?putRoom() crash!//0 returned!!!(2)"

   PROBLEM: A room record couldn't be written; 1 indicates the failure came
while writing the main body of the room's record, 2 the messages.  The fseek
failure indicates the system wasn't allowed to find the correct place in
CTDLROOM.SYS to write.

   SOLUTIONS?  This is very serious and may indicate a DOS problem or internal
bug.

------------

"Memory failure -- I need more memory!"

   PROBLEM: Citadel-86 couldn't allocate memory for an internal buffer.

   SOLUTION: Don't run such a large system or free up more space by killing
a TSR or shrinking a RAM disk.

------------

"Couldn't open the floor file for update!"

   PROBLEM: Citadel-86 tried to open the floor file for an update and failed.

   SOLUTION: This may be caused by accidentally deleting your CTDLFLR.SYS
file while in Outside Commands or something.  Or it may indicate you don't
have FILES=20 in your CONFIG.SYS.

------------

"?putFloor(): write failed!"

   PROBLEM: Citadel-86 tried to write to the floor file and failed.

   SOLUTION?  The only probable cause would be a problem with disk full or
with DOS.

------------

"?getNet-read fail(1)!!"
"?getNet-read fail(2)!!"
"?getNet-read fail(3)!!"

   PROBLEM: An attempt to read a net (node) record failed.  1 indicates the
failure occurred while reading the main body of the net record, 2 the room
sharing information, 3 the network-archive information.

   SOLUTION:  The causes could be

 o changing #SHARED-ROOMS without using Ease or DataChng.
 o changing #NET-ARCH-ROOMS etc.
 o trying to change both of the above at the same time.
 o internal bug.

------------

"?putNet-write fail(1)!!"
"?putNet-write fail(2)!!"

   PROBLEM: An attempt to write a net (node) record failed.  1 indicates the
failure occurred while writing the main body of the net record, 2 the room
sharing information.

   SOLUTIONS?  Well, best bets are a full disk, an internal bug or a problem
with DOS.

------------

"?nextMsgChar-read fail"

   PROBLEM: Citadel-86 tried to read a sector from the message base and
failed.

   SOLUTIONS?  Either you have a badly damaged CTDLMSG.SYS (the file has
been truncated in some way), or you've messaged with your #MESSAGEK parameter
without using Expand or Ease.

------------

"?startAt read fail"

   PROBLEM: Citadel-86 tried to change file position to read a message and
wasn't allowed to.

   SOLUTIONS?  See above error.

------------

"?putMsgChar-write fail"
"?putMsgChar-read fail"
"?ctdlmsg.sys write fail"

   PROBLEM: These two errors indicate, collectively, a failure to write a
message to disk.  The first and third indicate a failure occurred while
actually writing to disk, the second failure occurred while reading in part
of the message base in preparation for modification and writing.

   SOLUTIONS?  See above error.

------------

"putMessage -- couldn't open direct mail file!"
"putMLNet crash"

   PROBLEM: Citadel-86 attempted to open a network Mail> file (*.ML) and
failed; Citadel-86 couldn't write to the network Mail> file.

   SOLUTIONS?  The disk may be full; #NETAREA may have disappeared (unlikely,
though); internal bug.

------------

"putMessage -- couldn't open route mail file!"

   PROBLEM: Citadel-86 attempted to open a network routed Mail> file (R*.*)
and failed.

   SOLUTIONS?  See above error.

------------

"bad op: start=<number> cycles=<number>"

   PROBLEM: An internal Citadel-86 routine contained a bad op-code.

   SOLUTIONS?  Memory is being corrupted by something.  Try a new version of
Citadel-86, see if you can get it to reproduce (and then report it), try
changing something in your system.

------------

"Dependent variables mismatch!"

   PROBLEM: Citadel-86 detected a a mismatch between the room name in the
internal index and the name of the room read off of disk.

   SOLUTIONS?  Internal bug.  Report it, see if you can reproduce it at will.

------------

"shared rooms: #2"
"shared rooms: #1"

   PROBLEM: A bad shared room status was detected.

   SOLUTIONS?  An examination of the netlog.sys should indicate what room the
crash occurred for; also, editing the implicated node and using the 'R'ooms
command should show which room looks odd.  Unshare the room and then reshare
it.

------------

"ctdlvnet.sys is missing!!"
"ctdlvrm.sys is missing!"
"ctdlvnet.sys is missing!!"

   PROBLEM: These indicate something's wrong in the virtual rooms.

   SOLUTIONS?  Check to make sure FILES=20 is in your CONFIG.SYS.  See if
the files exist in the VIRTUAL directory.

------------

"crashout: unknown case in switch statement"

   PROBLEM: Citadel-86 is upset about handling the calllog.sys.

   SOLUTIONS?  This is an internal bug, please report it and all circumstances.

------------

"CallMsg error!"

   PROBLEM: Citadel-86 can't open an audit file.

   SOLUTIONS?

 o The most likely problem is FILES=20 is not in your CONFIG.SYS.  Triple
check this possibility.  Or maybe you didn't reboot after setting it in your 
CONFIG.SYS?
 o Citadel-86 "moved" itself out of its directory and failed to move back (very
unlikely), which is an internal bug.

------------

"putSLNet crash"

   PROBLEM: Citadel-86 can't write to a sendfile file.

   SOLUTIONS?  Beats us.


------------

NON-FATAL WARNING MESSAGES:
   These messages typically only show up either at the console or on the
user's screen and the console.

------------

"URP IN DOMAINS!"

   PROBLEM: Internal bug in Citadel-86.  Indicates the system thought was
absolutely certain it is a domain server for some domain, but upon checking
discovered to its dismay that it is not.

   SOLUTIONS? Report it and all details.  Attempt to reproduce.

------------

"WARNING: badly formed ctdldmhd.sys."

   PROBLEM: Your CtdlDmhd.Sys file (see Network3.Man) has one or more lines
in it that Citadel-86 couldn't really do much with.

   SOLUTIONS? Fix them if you built the file yourself, else contact the
person who made the file and inform them of the problem.  Be aware that maybe
the file has just been trashed up.  Also, if you have a CtdlDmhd.Lcl file
in place, the problem may lie in there rather than in CtdlDmhd.Sys.

------------

"INTERNAL FILE ERROR!"

   PROBLEM: Citadel-86 tried to open a compressed file prepatory to reading
information from it and failed.

   SOLUTIONS? At this point, Citadel-86 should already have verified the file
exists, so why the opening the file failed indicates a bug of some sort.

------------

"Sorry, no room in system."

   This message is displayed to a TWITted user attempting to create a room.

------------

"Your date is unintelligible."

   This message is displayed to a user who attempted to read messages since
or before a given date and entered a date Citadel-86 couldn't decode.  The
correct format for a date is yymmmdd, where yy is optional, mmm is the
month and is text, not numeric, and need only be so long as to be unique
(the longest this requires is three letters in the case of July and June).

------------

"System error with mkdir!"

   This message is displayed when attempting to create a temporary work
directory.  The only probable cause is a full disk, since Citadel-86 checks
for files/directories which might interfere with creating a temporary
directory (i.e., the name used for the temporary directory is dynamic).

------------

"SetSpace failure, aborting!"

   This message indicates the system attempted to change directory to the
directory attached to the current room prepatory to transmitting a file
using an external protocol and failed.  Presumably, the existence of the
directory should have been verified, so this error would probably indicate
a bug of some sort.

------------

"Sorry, the list of files would be too long.  Try again without a date
            specification."
"Sorry, the list of files would be too long."

   These two messages are caused by the same problem.  Citadel-86 transmits
files via external protocols by executing the named program with the file
names (wildcards are expanded).  Unfortunately, DOS has a command line
limitation of 128 characters, so if too many file names result from the
user's file specification, Citadel-86 cannot run the program safely.  If there
is a date specification attached to the download, then Citadel-86 simply
aborts.  If not, Citadel-86 again tries to build a command line, this time
with an unexpanded file-spec (i.e., what the user typed in).  If overflow
once again occurs, Citadel-86 gives up.  If it does work, Citadel-86 will
execute the external program, trusting the program can expand its wildcards.

------------

"Ooops, couldn't find <msg#>"

   This message indicates your system's message base (CTDLMSG.SYS) has
suffered some sort of damage, or Citadel-86 is confused.  Running CONFG
on your system may fix it if Citadel-86 is confused (although you may have
to delete CHKPT to ensure a complete reconfigure), but if there's real
damage (caused by disk errors or Citadel-86 bugs (the latter unlikely)),
then you'll just have to wait until Citadel-86 overwrites the damaged error.

   Since we've not seen Citadel-86 destroy its own message base, you may want
to consider have your hardware checked.

------------

"Unexpected internal error, please report it!"

   This message indicates a bug in Citadel-86's Mail system.  Please try
to reproduce it and then report it.

------------

"Internal error in mail!"

   This error indicates another bug in Citadel-86's Mail which should
be reported.  This time, Citadel-86 was saving a Mail message and suddenly
discovered the user the Mail was intended for can no longer be found
despite earlier verification!

------------

"Internal error, couldn't identify <string>"

   This error, which can involve both users and system names, is generated
by the Who Else processing and, once again, indicates an internal bug
which should be reproduced and then reported it.

------------

"ERROR: Couldn't open output file <filename>"

   This message is occurs when Citadel-86 attempted to open a file to Journal
(manual) or Archive (automatic) a message and the open failed.  This
normally indicates a directory or other special object in DOS already
exists with that name, or you tried to open a file which can't exist
(usually because the directory you specified also doesn't exist).  Please
use a different name.

------------

"C-86 internal error with doors.  Sorry."

   Citadel-86 tried to open a transitional data file (to save data in while
a door is up) and failed.  Maybe your disk is full?  Maybe we have a bug
in Citadel-86.

------------

"?ERROR CREATING!"

   This message occurs when Citadel-86 tries to create a directory for some
reason (usually during System Operator activity).  The only probable cause,
offhand, is a full disk.

------------

"That's not a directory!"

   Citadel-86 asked you for a directory name and you gave it the name of
a file or something else which it couldn't process.  BAAAAAAAAAAAAAAAD
Sysop!

------------

"?Directory not present! (internal error)"

   This message indicates your CTDLDIR.SYS file is messed up or missing
completely.  Check on it.

------------

"?Directory not present!"

   This message indicates the directory attached to the current room
doesn't exist.  You probably deleted it while Citadel-86 was off-line.
Either recreate it or change the room's setting.

------------

"Unknown problem with filetagging!"

   FILEDIR.TXT could not be opened for update.  Disk full?  Bug?

------------

"buffer overflow"

   The user tried to type in more than 7500 characters of text.  Split the
message.

------------

"?not found."

   User using replace string typed in a string which isn't in the current
message.

------------

"?Overflow!"

   User using replace string typed in a replacement string which, if allowed,
would cause an overflow of the message buffer.  Split the message.

------------

"Sorry, already sharing <number> rooms with <nodename>"

   You've reached your #SHARED-ROOMS setting for the named node.  Either
give up or use Ease or DataChng to increase #SHARED-ROOMS.

------------

"Couldn't open %s for update?"

   This happens when you are attempting to send some files to another system
and Citadel-86 couldn't open a file to handle the administration.  Full disk,
perhaps?  Bug?

------------

"Sorry, reserved name"

   You typed in &L for a nodename.  This is reserved to indicate "All Local
Systems."

------------

"There are only 31 nets to choose from."

   Read NETWORK3.MAN on Member Nets.

------------

"Couldn't append to <filename>????"

   You wanted to request some files but this error occurred.  Full disk?
Bug?

------------

"Unknown problems with netMail: author=-<name>-, target=-<name>-,
context=-<stuff>-.  System was <name>."

   This error, which will show up in the Aide> room, indicates your system
attempted to send Net Mail to another system (named in the message), but
failed for some completely unknown reason.  The author name indicates who
authored the message, the target is the name of the person the mail was
supposed to be delivered to, and the context is supposed to contain stuff
allowing debug of the problem.

   The only current case in which this may show up is when someone sends
netmail somewhere else but to a recipient who doesn't exist, and then is
deleted from the sending system before the mail is delivered.  This can also
happen on older systems (pre-V3.32) with accounts forwarding mail incorrectly
onward, when the mail being forwarded is netmail and the sender on real
originating system doesn't happen to exist on the intermediate system (that
is, the one which found the warning message in the Aide> room).

------------

THE NETLOG

   These messages may appear from time to time in your netlog if you happen
to run a netlog.  Not all netlog messages are covered here, only the unusual
messages.

------------

"Couldn't switch to WXMODEM"
"No YMODEM"

   These two messages indicate Citadel-86 couldn't switch from XMODEM, the
default low-level protocol, to a different (and presumably better)
protocol.  These messages will be seen most often when dealing with
older Citadel-86 variants such as STadel or very old Citadel-86 systems.

------------

"No compaction"

   For some reason Citadel-86 couldn't enable message compaction when netting
with another system.  As with the two previous messages, this'll be seen
with old variants and elderly Citadel-86 systems.

------------

"ITL_send failure <number>, mode <number>"

   This message indicates an unexpected interruption of service at the
lowest level of the network protocol, specifically during transmission (as
opposed to reception) of data or control information.  There are a number of
causes for messages of this sort:

 o Some versions of STadel and STadel-variants do not handle role-reversal
termination correctly.  In this case there is nothing to worry about, all
data should transfer cleanly despite the unfortunate glitch.

 o Line noise.  There is no guarantee that all data transferred.

 o Disk full conditions sometimes cause this.

 o Etc.

   The following are the specific values and their meanings for the two
numbers that can show up.

   The FAILURE code (first number):

 2 -- The receiver cancelled the transmission.  This is highly unusual,
since a cancellation of a transmission should take place at a far higher
level.

 3 -- The transmission never even started; the initialization sequence for
whatever protocol is in use (NAK for XMODEM, for example) was never
received during the timeout period.

 4 -- General failure code, could mean anything from the system was being
fussy to full disks.

 9 -- Carrier was lost.

   The MODE code (second number) can only be one of two numbers:

 1 -- The failure occurred while attempting to start a transmission; that is,
Citadel-86 was prepared to send information and the other end crapped out
without even trying to initialize the transmission.  Usually, Mode 1 failures
occur with a failure code of 9.

 2 -- The failure occurred during or at the end of the transmission.

------------

"Msg from unknown node <system name> @<system id>",

   During vortex handling, Citadel-86 displays the name and ID of every
system your installation doesn't know about which had a message in
the current batch to be processed.

------------

"<message id> from <system name> rejected."

   This indicates a message is thought to have been vortexed and was
thus rejected.  The message may be in a file named DISCARD.

------------

"Couldn't create <filename>!!"

   Citadel-86 was going to create a vortex record for some system and
couldn't open the file for recording the data.  Is your disk full?

------------

"No recipient: <name>"

   Net Mail> was sent to a non-existent user on your system.

------------

"WARNING: Can't open <filename>!!!!"

   Citadel-86 couldn't open a temporary buffer for generating the summary
Aide> message.  Disk full?  FILES=20 in your CONFIG.SYS?

------------

"Rejecting <system name> (<system id>)"

   The other system wanted to use you to route to another system and
you refused for some reason, most likely route flags.

------------

THE DOMAIN.LOG

   These messages may appear from time to time in your domain.log (which in
turn is located in your #DOMAINAREA directory).

------------

"Mail for unknown system <system name>."

   PROBLEM: Your system is a domain server.  Your system was asked to deliver
mail to a system in a domain you serve which you couldn't identify.

   SOLUTIONS? If the system really is unknown, not much.  You can delete the
mail if you like; mail addresses are located in the first 42 bytes of each
file, and killing a file won't hurt.  At the current time there is no mechanism
for automatically sending negative acknowledgements back to the sender.

   If, on the other hand, the system name was merely misspelled, you can use
your ALIASES.SYS file to redirect the mail.  See Network3.Man for more detail
on ALIASES.SYS.

   Finally, if it happens that the target system is located in some other domain
which you do not serve (user picked wrong domain), you can use an editor to
edit your MAP.SYS file to redirect the mail.  Simply find the entry containing
the bad domain name, correct it, and bring your system back up (oh yeah, you
should bring your system down before doing this sort of stuff).

------------

"Don't know how to reach domain <domain name>."

   PROBLEM: This is a very rare message.  Your system has no mail hub defined
and has received mail for a domain it knows nothing about.

   SOLUTIONS? Define a mail hub.  Find out who serves the domain and make
appropriate adjustments to your system to get it delivered.  If the domain
is just misspelled, consult the last paragraph of the above error message.

------------
