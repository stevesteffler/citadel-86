;       CitZen  - Citadel 2.1X Drivers for Zenith Z-100
;       Derived from the Kermit 2.29 drivers for the Zenith Z-100.
;       Any bugs are probably those of Eric Brown, as he wrote the
;       derivation.
;
; 88May15 HAW  Doubled size of output buffer to fix apparent bug.

BIOS_SEG SEGMENT AT 40H         ; Define segment where BIOS really is
        ORG     4*3
BIOS_PRINT  LABEL FAR
        ORG     6*3
BIOS_CONFUNC LABEL FAR          ; CON: function
BIOS_SEG ENDS

; Function codes for BIOS_xxxFUNC
CHR_WRITE       EQU     0       ; Write character
CHR_READ        EQU     1       ; Read character
CHR_STATUS      EQU     2       ; Get status
  CHR_SFGS      EQU     0       ; Get status subfunction
  CHR_SFGC      EQU     1       ; Get config subfunction
CHR_CONTROL     EQU     3       ; Control function
  CHR_CFSU      EQU     0       ; Set new configuration parameters
  CHR_CFCI      EQU     1       ; Clear input buffer

; serial port information
TSRE    EQU     004H
THBE    EQU     001H
DTR     EQU     002H
DTROFF  EQU     0fdH            ; not dtr
RTS     EQU     020H
RTSOFF  equ     0dfh            ; not rts
DCD     EQU     040H
RDA     EQU     002H
J1_ADDR EQU     0e8H
J2_ADDR EQU     0ecH
PDATA   EQU     0
PSTATUS EQU     1
PMODE   EQU     2
PCOMM   EQU     3
TXON    EQU     001H
TXOFF   EQU     0feH
RXON    EQU     004H
RXOFF   EQU     0fbH
MODE1   EQU     04dH
MODE2   EQU     030H    ;  must be ORed with appropriate baud rate
Z8259   EQU     0f2H
EOI     EQU     020H
J1INT   EQU     68
J2INT   EQU     69
BUFILEN EQU     1200
BUFOLEN EQU     1200

mntrgh  equ     BUFILEN/10      ; trigger is 1/10th of input buffer
false   equ     0
true    equ     1

dos     equ     21h

; Structure definitions.

; Modem information.
mdminfo struc
mddat   dw 0            ; Default to port 1.
mdstat  dw 0            ; Ditto.
mdcom   dw 0            ; Here too.
mden    db 0
mddis   db 0
mdmeoi  db 0
mdintv  dw 0
mdminfo ends

; Port Information.
prtinfo struc
baud    dw 0            ; Default baud rate.
parflg  db 0            ; Parity flag (default none.)
stpflg  db 0            ; stop bits.
wordlen db 0            ; word length
floflg  db 0            ; If need flow control during file x-fer.
flowc   dw 0            ; characters to do flow ctl with.
prtinfo ends


_DATA   segment word public 'DATA'

portval dw      port1
port1   prtinfo <8, 0, 1, 8, 0, 0>      ; 1200 baud, 8/n/1, no flow ctl.
port2   prtinfo <8, 0, 1, 8, 0, 0>      ; 1200 baud, 8/n/1, no flow ctl.

xofsnt  db      false
xofrcv  db      false
brkval  db      0               ; What to send for a break.
brkadr  dw      0               ; Where to send it.

bddat   label   word
        dw      45, 0           ; 45.5 baud
        dw      50, 1           ; 50 baud
        dw      75, 2           ; 75 baud
        dw      110, 3          ; 110 baud
        dw      135, 4          ; 134.5 baud
        dw      150, 5          ; 150 baud
        dw      300, 6          ; 300 baud
        dw      600, 7          ; 600 baud
        dw      1200, 8         ; 1200 baud
        dw      1800, 9         ; 1800 baud
        dw      2000, 10        ; 2000 baud
        dw      2400, 11        ; 2400 baud
        dw      4800, 12        ; 4800 baud
        dw      9600, 13        ; 9600 baud
        dw      19200, 14       ; 19200 baud
        dw      38400, 15       ; 38400 baud
        dw      0ffffh,  -1     ; invalid

; variables for serial interrupt handler

count   dw      0               ; Number of chars in int buffer.
_buffin  db      BUFILEN dup(?)  ; input buffer
bufibeg dw      0
bufiend dw      0
buffout db      BUFOLEN dup(?)  ; output buffer
bufobeg dw      0
bufoend dw      0
portadr dw      0
intin   db      0               ; port int installed flag
oldseg  dw      0
oldoff  dw      0
_DATA   ends

public _buffin

NULL    segment para public 'BEGDATA'
NULL    ends
CONST   segment word public 'CONST'
CONST   ends
_BSS    segment word public 'BSS'
_BSS    ends

DGROUP  group NULL, _DATA, CONST, _BSS

_citzen_text    segment byte public 'CODE'
        assume  cs:_citzen_text, ds:DGROUP, es:nothing

; Put the char in AH to the serial port.  

; _mPutch(ch)
;       char ch;
;       Put the char ch to the serial port.
;       This assumes the port has been initialized.  
;       Should honor xon/xoff if enabled.
;       Returns 0 on success, 1 if failure.

public  _mPutch
_mPutch proc    far
        push    bp
        mov     bp, sp
        mov     ah, [bp+6]
        mov     bp,portval
        cmp     ds:[bp].floflg,0 ; Are we doing flow control.
        je      outch2          ; No, just continue.
        xor     cx,cx           ; clear counter
        cmp     ah,byte ptr [bp].flowc      ; sending xoff? [jrd]
        jne     outch1              ; ne = no
        mov     xofsnt,false        ; supress xon from chkxon buffer routine
outch1: cmp     xofrcv,true     ; Are we being held?
        jne     outch2          ; No - it's OK to go on.
        loop    outch1          ; held, try for a while
        mov     xofrcv,false    ; timed out, force it off and fall thru.
outch2: push    bx              ; Save register.
        mov     bx,bufoend      ; get pointer to end of que
        mov     byte ptr buffout[bx],ah  ; put char in it
        inc     bx              ; point to next spot in que
        cmp     bx,BUFOLEN      ; looking at end of que ?
        jne     outch3          ; no, OK
        xor     bx,bx           ; yes, reset pointer
outch3: cli
        mov     bufoend,bx      ; store new value
        mov     bx,dx
        mov     dx,portadr
        add     dx,PCOMM
        in      al,dx
        test    al,TXON         ; TX already on ?
        jnz     outch4          ; yes, OK
        or      al,TXON         ; no, turn it on
        out     dx,al           ;   it's on
outch4: mov     dx,bx
        sti                     ; done with 2661
        pop     bx
        mov     ax, 0
        pop     bp
        ret
_mPutch endp


; Set the baud rate for the current port.  
;       AX = raw baud rate (300, 1200, etc.)

public  dobaud
dobaud  PROC    NEAR
        push    bx
        lea     bx,  bddat
baud_loop:
        cmp     ax, ds:[bx]     ; same?
        je      found
        cmp     ds:[bx], 0ffffh ; end?
        je      dobaud_xit      ; yes.
        add     bx, 4           ; next!
        jmp     baud_loop
found:
        mov     ax, ds:[bx+2]   ; get real baud rate code
dobd0:  push    dx              ; need to use it
        push    ax              ; save baud rate
        mov     dx,portadr      ; get addr to send it
        add     dx,PMODE
        mov     al,cl           ; get synthesized mode1 word
        cli                     ; none while setting 2661
        out     dx,al           ; mode reg 1/2
        pop     ax              ; get baud back
        and     al,0fH          ; make sure it's clean
        or      al,MODE2        ; make complete mode 2/2 command
        out     dx,al           ; set mode reg 2/2
        sti                     ; done with 2661
        pop     dx              ; restore it
dobaud_xit:
        pop     bx
        ret
dobaud  ENDP

; synthesize appropriate mode1 word in cl given
; stop bits in cl, parity in bl, and word length in al.
; note - this routine assumes you know ALL ABOUT an 2661
; serial chip.  Beware!

init_term       proc    near
        sub     al, 5           
        and     al, 3h          ; mask 5-8 down to 0-3.
        shl     al, 1           ; put over into appropriate slot
        shl     al, 1
        or      al, 01b         ; set Async mode
        cmp     bl, 'O'         ; odd parity?
        jne     notodd
        or      al, 010000b     ; set odd parity
notodd:
        cmp     bl, 'E'         ; even?
        jne     noteven
        or      al, 110000b     ; set even parity
        ; if we've gotten this far, we're going to assume No parity.
noteven:
        cmp     cl, 1           ; one stop bit?
        jne     notone
        or      al, 01000000b   ; yes, set one stop bit
notone:
        cmp     cl, 2           ; two stop bits?
        jne     nottwo
        or      al, 11000000b   ; yes, set two stop bits
nottwo:
        cmp     cl, 2           ; other strange value?
        jbe     done
        or      al, 10000000b   ; yes, assume 1.5.
done:
        mov     cl, al          ; put into CL for safekeeping
        ret
init_term       endp

; int
; mGetch ()
;   returns character if available, -1 if not.
;

public  _mGetch
_mGetch  proc    far
        call    chkxon          ; see if we need to xon
        mov     dx,bufiend      ; compute number of chars in
        sub     dx,bufibeg      ;   input que
        jge     prtch1          ; is it wrapped around
        add     dx,BUFILEN      ; yes, make it +
prtch1: cmp     dx,0            ; anything in there ?
        jne     prtch3          ; ne = yes. [jrd]
        mov     ax, -1          ; nope
        jmp     short prtch4
prtch3: push    bx              ; yes, get the char
        mov     bx,bufibeg      ; get the position
        mov     al,byte ptr _buffin[bx]  ; get the char
        inc     bx              ; bump the position ptr
        cmp     bx,BUFILEN      ; wrap it ?
        jne     prtch2
        xor     bx,bx           ; yes, reset pointer
prtch2: mov     bufibeg,bx      ; store new value
        dec     dx              ; we took one char out
        mov     count,dx        ; save (does anyone use this??)
        pop     bx
prtch4:
        ret
_mGetch endp

; local routine to see if we have to transmit an xon
chkxon  proc    near
        push    bx
        mov     bx,portval
        cmp     [bx].floflg,0   ; doing flow control?
        je      chkxo1          ; no, skip all this
        cmp     xofsnt,false    ; have we sent an xoff?
        je      chkxo1          ; no, forget it
        cmp     count,mntrgh    ; below trigger?
        jae     chkxo1          ; no, forget it
        mov     ax,[bx].flowc   ; ah gets xon
        xchg    ah, al
        push    ax
        call    _mPutch         ; send it
        add     sp, 2           ; pop args
        mov     xofsnt,false    ; remember we've sent the xon.
chkxo1: pop     bx              ; restore register
        ret                     ; and return
chkxon  endp


; simple routine to insure that the port has RXON and DTR high
;  assumes int are off
porton  proc    near
        push    dx
        push    ax
        mov     dx,portadr
        add     dx,PCOMM
        in      al,dx
        or      al,RXON+DTR+RTS
        and     al,3fh          ; make DAMN sure that we're not in 
        out     dx,al           ; some odd loopback state.
        pop     ax
        pop     dx
        ret
porton  endp

; Clear the input buffer. This throws away all the characters in the
; serial interrupt buffer.  This is particularly important when
; talking to servers, since NAKs can accumulate in the buffer.
; Returns normally.

clrbuf  PROC    NEAR
        cli
        push    bx
        xor     bx,bx
        mov     bufoend,bx
        mov     bufobeg,bx
        mov     bufiend,bx
        mov     bufibeg,bx
        pop     bx
        mov     count,0
        sti
        ret
clrbuf  ENDP

; routine to retrieve current int vector
;  inputs:  al = int number
;  outputs: cx = seg for current isr
;           dx = offset for current isr
getivec proc    near
        push es                 ; save registers
        push bx
        mov ah,35H              ; Int 21H, function 35H = Get Vector.
        int dos                 ; get vector in es:bx
        mov     cx,es           ; addr of org vector (seg)
        mov     dx,bx           ;   and offset
        pop bx
        pop es
        ret
getivec endp

; routine to set int vector
;  inputs:  ah = int number
;           cx = seg for isr
;           dx = offset for isr
setivec proc    near
        push    ds              ; save ds around next DOS call.
        mov     ds,cx
        mov     ah,25H          ; set interrupt address from ds:dx
        int     dos
        pop     ds
        ret
setivec endp


; Reset the serial port.  This is the opposite of serini.  Calling
; this twice without intervening calls to serini should be harmless.
; Returns normally.

public  _mClose
_mClose proc    far
        cmp     intin,0         ; is any isr installed
        je      serr2           ; no, all done
        push    dx
        push    ax
        push    cx

        mov     dx, portadr     ; drop DTR and RTS
        add     dx, PCOMM       ; get command port
        in      al, dx
        and     al, RTSOFF and DTROFF   ; clear dem bits
        out     dx, al

        mov     ax,J2INT        ; guess it's J2
        cmp     intin,2         ; yes,
        je      serr1           ;   reset it
        mov     ax,J1INT        ; no, must be J1     
serr1:  mov     cx,oldseg       ; original isr
        mov     dx,oldoff       ;   address
        call    setivec         ; do it
        mov     intin,0         ; show nothing installed
        pop     cx
        pop     ax
        pop     dx
serr2:
        ret                     ; All done.
_mClose endp

; initialization for using serial port.  This routine performs
; any initialization necessary for using the serial port, including
; setting up interrupt routines, setting buffer pointers, etc.
; Doing this twice in a row should be harmless (this version checks
; a flag and returns if initialization has already been done).
; SERRST below should restore any interrupt vectors that this changes.
; Returns normally.

initstk struc
        dw      ?
        dd      ?
iport   dw      ?
ibaud   dw      ?
iparity dw      ?
istop   dw      ?
ilen    dw      ?
ixon    dw      ?
initstk ends

public  _mInit
_mInit  proc    far
        push    bp
        mov     bp, sp
        mov     ax, [bp].iport
        cmp     ax, 2
        jne     seri2           ; setup for J1
        cmp     intin,2
        jne     seri0
        jmp     seri_xit        ; J2 already set up
seri0:
        cmp     intin,1
        jne     seri1           ; J1 currently installed
        call    _mClose         ; de-install current int
seri1:
        mov     al,J2INT
        call    getivec
        mov     oldseg,cx
        mov     oldoff,dx
        mov     cx,cs
        lea     dx, serisr
        mov     al,J2INT
        call    setivec
        mov     portadr,J2_ADDR
        mov     brkadr,J2_ADDR+PCOMM
        call    clrbuf
        call    porton
        mov     intin,2         ; show J2 installed
        jmp     short seri_done ; finish installation
seri2:
        cmp     intin,1
        je      seri_xit        ; J1 already set up
seri3:
        cmp     intin,2
        jne     seri4           ; J2 currently installed
        call    _mClose         ; de-install current int
seri4:
        mov     al,J1INT
        call    getivec
        mov     oldseg,cx
        mov     oldoff,dx
        mov     cx,cs
        lea     dx, serisr
        mov     al,J1INT
        call    setivec
        mov     portadr,J1_ADDR
        call    clrbuf
        call    porton
        mov     intin,1         ; show J1 installed
seri_done:
        mov     cx, [bp].istop  ; get stop bits,
        mov     bx, [bp].iparity; parity, 
        mov     ax, [bp].ilen   ; and word length.
        call    init_term       ; synthesize appropriate mode word
                                ; in cx
        mov     ax, [bp].ibaud  ; get baud rate

        cmp     ax, 0
        je      seri_nobaud

        call    dobaud          ; set it
seri_nobaud:
        mov     bx, portval
        mov     ax, [bp].ixon   ; set xon flag correctly.
        mov     [bx].floflg, al ;
        xor     ax, ax
seri_xit:
        pop     bp
        ret
_mInit  endp


; _mChange (baud, parity, stop, len, xon)
; reinitialize serial port.  This routine alters the serial port
; settings for the current port.
; This can be quite dangerous if you're actually running a session
; when you do this.

chgstk  struc
        dw      ?
        dd      ?
cbaud   dw      ?
cparity dw      ?
cstop   dw      ?
clen    dw      ?
cxon    dw      ?
chgstk  ends

public  _mChange
_mChange        proc    far
        push    bp
        mov     bp, sp
        mov     cx, [bp].cstop  ; get stop bits,
        mov     bx, [bp].cparity; parity, 
        mov     ax, [bp].clen   ; and word length.
        call    init_term       ; synthesize appropriate mode byte
                                ; in cl
        mov     ax, [bp].cbaud  ; get baud rate
        call    dobaud          ; set it
        mov     bx, portval
        mov     ax, [bp].cxon   ; set xon flag correctly.
        mov     [bx].floflg, al ;
        xor     ax, ax
        pop     bp
        ret
_mChange        endp

; mHasch () - return # of chars in input buffer
;

public  _mHasch
_mHasch proc    far
        mov     ax,bufiend      ; compute number of chars in
        sub     ax,bufibeg      ;   input que
        jge     mhasch1         ; is it wrapped around
        add     ax,BUFILEN      ; yes, make it +
mhasch1:
        ret
_mHasch endp

; mHasout () - returns # of characters in output buffer
;

public  _mHasout
_mHasout        proc    far
        mov     ax,bufoend      ; compute number of chars in
        sub     ax,bufobeg      ;   output que
        jge     mHasout1        ; is it wrapped around
        add     ax,BUFOLEN      ; yes, make it +
mHasout1:
        ret
_mHasout        endp

; the serial port interrupt service routine
;  this routine does int driven input and output
;   once installed, it displaces the Z-100 serial isr
serisr: push    ax                      ; Save regs
        push    bx
        push    cx
        push    dx
        push    ds                      ; save data seg
        mov     ax,seg DGROUP           ; set our
        mov     ds,ax                   ;  data seg

        mov     dx,portadr
        mov     cx,dx
        add     dx,PSTATUS
        in      al,dx           ; Get port status
        mov     ah,al           ; Save it
        test    ah,RDA          ; check for data available
        jz      isr2            ;   No, skip

        mov     dx,cx
        in      al,dx           ; get the data
        mov     bx,bufiend      ; get where to put it
        mov     byte ptr _buffin[bx],al ; stick it in the que
        inc     bx              ; bump que pointer
        cmp     bx,BUFILEN      ; pointing to end of que?
        jne     isr1
        xor     bx,bx           ; reset pointer
isr1:   mov     bufiend,bx      ; store new pointer

isr2:   test    ah,TSRE+THBE    ; ready to send a char?
        jz      isr5            ;  no, almost done
        mov     bx,bufobeg      ; get pointer to end of output buffer
        cmp     bx,bufoend      ; buffer empty?
        jz      isr4            ; yes, turn transmitter off
        mov     al,byte ptr buffout[bx] ; get char to send
        mov     dx,cx
        out     dx,al           ; send it
        inc     bx              ; point to next char to send
        cmp     bx,BUFOLEN      ; pointing to end of que?
        jne     isr3
        xor     bx,bx           ; reset pointer
isr3:   mov     bufobeg,bx      ; save it
        jmp     short isr5

isr4:   mov     dx,cx
        add     dx,PCOMM
        in      al,dx           ; get current mode
        and     al,TXOFF        ; turn xmitter off
        out     dx,al           ; do it.

isr5:   mov     al,EOI          ; Tell interrupt controller
        out     Z8259,al        ;   that interrupt serviced

        pop     ds
        pop     dx
        pop     cx
        pop     bx              ; restore regs
        pop     ax

        iret

_citzen_text    ends
        end
