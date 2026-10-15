; DUMPROM.COM -- copies the firmware of a Eureka A4 into a file, so that its
; owner can run it in Eureka A4 Emulator.  See PRAVNE.md, section 4 and 5:
; the copy is for the owner of the machine it comes from, and for no one else.
;
; Build: tools/dumprom/build.sh (GNU binutils for Z80, -march=z180).
;
; What it does:
;  1. Reads the MMU (CBAR, BBR, CBR) and works out where its own buffer lies
;     in physical memory -- never assumed, because the Advanced English model
;     keeps the owner's telephone directory where a Standard one has RAM.
;  2. Reads every 4 KiB page of banks 0-4 by DMA, twice, and keeps a
;     signature of it.  A bank is "empty" when every page holds one repeated
;     byte, "unstable" when a page reads differently the second time, and a
;     "mirror" when all its pages match those of an earlier bank.  Only a
;     bank that is none of these counts as data.
;  3. Banks 0-3 all with their own data: every 27C512 build, which is the
;     256 KiB the emulator loads -> A4ROM.DMP (262144 bytes).  Bank 4 is
;     never written then: it is either nothing, or the owner's own RAM (the
;     service-fitted module, or page 0 and TPA of the Advanced model).
;     Banks 0-1 with data, 2-3 without, 4 with: the 27C256 build, whose ROM
;     goes on at 40000h (MEMMAP.E) -> A4ROM256.DMP (327680 bytes).
;     Anything else is reported bank by bank and nothing is written.
;
; The program is ours alone: CP/M BDOS calls and HD64180 registers are
; interfaces, named as the Hitachi HD64180 manual names them.

        .equ    BDOS, 0x0005
        .equ    CONOUT, 2               ; BDOS: console output, E = character
        .equ    PRINT, 9                ; BDOS: print string up to '$'
        .equ    FCLOSE, 16
        .equ    FDELETE, 19
        .equ    FWRITE, 21              ; write sequential, 128 bytes
        .equ    FMAKE, 22
        .equ    SETDMA, 26

        ; HD64180 internal I/O registers (DMA channel 0 and the MMU)
        .equ    SAR0L, 0x20
        .equ    SAR0H, 0x21
        .equ    SAR0B, 0x22
        .equ    DAR0L, 0x23
        .equ    DAR0H, 0x24
        .equ    DAR0B, 0x25
        .equ    BCR0L, 0x26
        .equ    BCR0H, 0x27
        .equ    DSTAT, 0x30
        .equ    DMODE, 0x31
        .equ    CBR, 0x38
        .equ    BBR, 0x39
        .equ    CBAR, 0x3a

        ; Memory above the program, not part of the file.  BUF is 4 KiB
        ; aligned, so it lies in one MMU area; build.sh checks that the code
        ; ends below it.  Every Eureka has TPA up to at least BFFFh.
        .equ    BUF, 0x2000
        .equ    BUF_PAGE, 0x02          ; BUF >> 12
        .equ    SIGS, 0x3000            ; 5 banks * 16 pages * 4 bytes
        .equ    SIGS2, 0x3200           ; the same pages read a second time
        .equ    STACK, 0x3800

        .equ    BANKS, 5
        .equ    PAGES, 16

        .text
start:
        ld      sp, STACK
        ld      de, msg_title
        call    print

        ; Every Eureka has a JP at 0008h; the same test as a4_check in the
        ; Technical Manual's A4.INC.
        ld      a, (0x0008)
        cp      0xc3
        ld      de, msg_not_eureka
        jp      nz, finish

        in0     a, (CBAR)
        ld      (v_cbar), a
        in0     a, (BBR)
        ld      (v_bbr), a
        in0     a, (CBR)
        ld      (v_cbr), a

        ; CBAR = F0h and BBR = 40h while a program runs: Advanced English
        ; (A4.INC, a4_check).
        ld      de, msg_standard
        ld      a, (v_cbar)
        cp      0xf0
        jr      nz, 1f
        ld      a, (v_bbr)
        cp      0x40
        jr      nz, 1f
        ld      de, msg_advanced
1:      call    print

        ; Physical 4 KiB page of BUF.  Below CBAR's low nibble is common
        ; area 0 (unmapped), below its high nibble the bank area (+BBR),
        ; above that common area 1 (+CBR).
        ld      a, (v_cbar)
        and     0x0f
        ld      c, a
        ld      a, BUF_PAGE
        cp      c
        jr      c, 3f
        ld      a, (v_cbar)
        rrca
        rrca
        rrca
        rrca
        and     0x0f
        ld      c, a
        ld      a, BUF_PAGE
        cp      c
        jr      c, 2f
        ld      a, (v_cbr)
        jr      4f
2:      ld      a, (v_bbr)
        jr      4f
3:      xor     a
4:      add     a, BUF_PAGE
        ; ROM ends at 3FFFFh on every 27C512 build and 4FFFFh on 27C256;
        ; RAM never lies below 40000h, so a buffer there means a map we do
        ; not know.
        cp      0x40
        jp      c, bad_map
        ld      c, a
        rrca
        rrca
        rrca
        rrca
        and     0x0f
        ld      (v_dar0b), a
        ld      a, c
        rlca
        rlca
        rlca
        rlca
        and     0xf0
        ld      (v_dar0h), a

        ld      de, msg_reading
        call    print

        ; Pass 1: signatures of all pages, and which banks vary at all.
        ; Every page is read twice: ROM reads the same each time, while a
        ; socket with no chip may return whatever floats on the bus, and
        ; noise that varies must not pass for ROM.
        ld      d, 0
5:      ld      e, 0
        xor     a
        ld      (v_varied), a
        ld      (v_unstable), a
6:      push    de
        ld      hl, SIGS
        ld      (v_sigbase), hl
        call    dmaread
        call    scan
        ld      a, (v_varied_page)
        or      a
        jr      z, 7f
        ld      (v_varied), a
7:      pop     de
        push    de
        ld      hl, SIGS2
        ld      (v_sigbase), hl
        call    dmaread
        call    scan
        pop     de
        push    de
        call    same_page
        pop     de
        jr      z, 12f
        ld      a, 1
        ld      (v_unstable), a
12:     inc     e
        ld      a, e
        cp      PAGES
        jr      nz, 6b
        ; data[d] = 1 if the bank varies, reads the same twice and matches
        ; no earlier bank
        ld      a, (v_varied)
        ld      (v_kind), a             ; 0 = empty, 1 = data
        ld      a, 0xff
        ld      (v_mirror), a
        ld      a, (v_unstable)
        or      a
        jr      z, 13f
        xor     a
        ld      (v_kind), a
        jr      9f
13:     ld      a, (v_kind)
        or      a
        jr      z, 9f
        ld      e, 0
8:      ld      a, e
        cp      d
        jr      z, 9f
        push    de
        call    same_banks
        pop     de
        jr      nz, 10f
        ld      a, e
        ld      (v_mirror), a
        xor     a
        ld      (v_kind), a
        jr      9f
10:     inc     e
        jr      8b
9:      ld      hl, v_data
        ld      a, l
        add     a, d
        ld      l, a
        jr      nc, 11f
        inc     h
11:     ld      a, (v_kind)
        ld      (hl), a
        push    de
        call    report_bank
        pop     de
        inc     d
        ld      a, d
        cp      BANKS
        jp      nz, 5b

        ; Which layout?
        ld      hl, v_data
        ld      a, (hl)                 ; bank 0
        inc     hl
        and     (hl)                    ; bank 1
        jr      z, unknown
        inc     hl
        ld      a, (hl)                 ; bank 2
        inc     hl
        ld      b, (hl)                 ; bank 3
        inc     hl
        ld      c, (hl)                 ; bank 4
        and     b
        jr      nz, layout512
        ld      a, (v_data + 2)
        or      b
        jr      nz, unknown
        ld      a, c
        or      a
        jr      z, unknown
        ld      a, 5
        ld      (v_nbanks), a
        ld      hl, name256
        ld      de, msg_layout256
        jr      write_file
layout512:
        ld      a, 4
        ld      (v_nbanks), a
        ld      hl, name512
        ld      de, msg_writing512
        jr      write_file

unknown:
        ld      de, msg_unknown
        jp      finish

bad_map:
        ld      de, msg_bad_map
        call    print
        ld      de, msg_cbar
        call    print
        ld      a, (v_cbar)
        call    print_hex
        ld      de, msg_bbr
        call    print
        ld      a, (v_bbr)
        call    print_hex
        ld      de, msg_cbr
        call    print
        ld      a, (v_cbr)
        call    print_hex
        ld      de, msg_crlf
        jp      finish

        ; HL = 11 byte name, DE = message.  Pass 2 reads the banks again and
        ; writes them 128 bytes at a time.
write_file:
        push    hl
        call    print
        pop     hl
        ld      de, fcb + 1
        ld      bc, 11
        ldir
        ld      hl, fcb + 12            ; extent, s1, s2, rc, map, cr
        ld      b, 24
1:      ld      (hl), 0
        inc     hl
        djnz    1b
        ld      de, fcb
        ld      c, FDELETE
        call    BDOS
        ld      de, fcb
        ld      c, FMAKE
        call    BDOS
        inc     a
        ld      de, msg_no_create
        jr      z, finish

        ld      d, 0
2:      ld      e, 0
3:      push    de
        call    dmaread
        ld      hl, BUF
        ld      b, 32
4:      push    bc
        push    hl
        ex      de, hl
        ld      c, SETDMA
        call    BDOS
        ld      de, fcb
        ld      c, FWRITE
        call    BDOS
        pop     hl
        pop     bc
        or      a
        jr      nz, write_error
        ld      de, 128
        add     hl, de
        djnz    4b
        pop     de
        inc     e
        ld      a, e
        cp      PAGES
        jr      nz, 3b
        inc     d
        ld      a, (v_nbanks)
        cp      d
        jr      nz, 2b

        ld      de, fcb
        ld      c, FCLOSE
        call    BDOS
        inc     a
        ld      de, msg_write_error
        jr      z, finish
        ld      de, msg_done
        jr      finish

write_error:
        pop     de
        ld      de, fcb
        ld      c, FCLOSE
        call    BDOS
        ld      de, msg_write_error

finish:
        call    print
        jp      0

; Reads the 4 KiB page E of bank D (physical D:E000h) into BUF by DMA
; channel 0, memory to memory in burst mode, which holds the CPU until the
; transfer is done.  Registers and bits: HD64180 manual, DMA chapter.
dmaread:
        di
        xor     a
        out0    (SAR0L), a
        ld      a, e
        rlca
        rlca
        rlca
        rlca
        and     0xf0
        out0    (SAR0H), a
        ld      a, d
        out0    (SAR0B), a
        xor     a
        out0    (DAR0L), a
        ld      a, (v_dar0h)
        out0    (DAR0H), a
        ld      a, (v_dar0b)
        out0    (DAR0B), a
        xor     a
        out0    (BCR0L), a
        ld      a, 0x10                 ; 1000h bytes
        out0    (BCR0H), a
        ld      a, 0x02                 ; memory to memory, burst
        out0    (DMODE), a
        in0     a, (DSTAT)
        and     0xef                    ; /DWE0 = 0: DE0 may be written
        or      0x40                    ; DE0 = 1: go
        out0    (DSTAT), a
        ei
        ret

; Signature of BUF into the slot of bank D, page E: two 16-bit sums, the
; second over the running first, so that order matters and a mirror is
; told from data that only adds up the same.  v_varied_page = 1 unless
; every byte equals the first.
scan:
        ld      a, d
        rlca
        rlca
        rlca
        rlca
        add     a, e                    ; bank * 16 + page < 80
        ld      l, a
        ld      h, 0
        add     hl, hl
        add     hl, hl
        ld      bc, (v_sigbase)
        add     hl, bc
        push    hl
        ld      hl, BUF
        ld      iy, v_first
        ld      a, (hl)
        ld      (iy+0), a
        ld      (iy+1), 0               ; v_varied_page
        ld      de, 0
        ld      ix, 0
        ld      bc, 0x1000
1:      ld      a, (hl)
        cp      (iy+0)
        jr      z, 2f
        ld      (iy+1), 1
2:      add     a, e
        ld      e, a
        jr      nc, 3f
        inc     d
3:      add     ix, de
        inc     hl
        dec     bc
        ld      a, b
        or      c
        jr      nz, 1b
        pop     hl
        ld      (hl), e
        inc     hl
        ld      (hl), d
        inc     hl
        push    ix
        pop     de
        ld      (hl), e
        inc     hl
        ld      (hl), d
        ret

; Z set when all 16 page signatures of bank D equal those of bank E.
same_banks:
        ld      a, d
        call    sig_addr
        push    hl
        ld      a, e
        call    sig_addr
        pop     de
        ld      b, PAGES * 4
1:      ld      a, (de)
        cp      (hl)
        ret     nz
        inc     de
        inc     hl
        djnz    1b
        ret

; Z set when page E of bank D read the same both times (SIGS and SIGS2).
same_page:
        ld      a, d
        rlca
        rlca
        rlca
        rlca
        add     a, e
        ld      l, a
        ld      h, 0
        add     hl, hl
        add     hl, hl
        push    hl
        ld      bc, SIGS
        add     hl, bc
        ex      de, hl
        pop     hl
        ld      bc, SIGS2
        add     hl, bc
        ld      b, 4
1:      ld      a, (de)
        cp      (hl)
        ret     nz
        inc     de
        inc     hl
        djnz    1b
        ret

; HL = SIGS + A * 64
sig_addr:
        ld      l, a
        ld      h, 0
        add     hl, hl
        add     hl, hl
        add     hl, hl
        add     hl, hl
        add     hl, hl
        add     hl, hl
        ld      bc, SIGS
        add     hl, bc
        ret

; "bank N data", "bank N empty", "bank N unstable" or "bank N same as
; bank M" for bank D.
report_bank:
        ld      a, d
        add     a, '0'
        ld      (msg_bank + 5), a
        ld      de, msg_bank
        call    print
        ld      a, (v_unstable)
        or      a
        ld      de, msg_unstable
        jr      nz, 1f
        ld      a, (v_kind)
        or      a
        ld      de, msg_data
        jr      nz, 1f
        ld      a, (v_mirror)
        cp      0xff
        ld      de, msg_empty
        jr      z, 1f
        add     a, '0'
        ld      (msg_same + 13), a
        ld      de, msg_same
1:      jp      print

print:
        ld      c, PRINT
        jp      BDOS

print_hex:
        push    af
        rrca
        rrca
        rrca
        rrca
        call    1f
        pop     af
1:      and     0x0f
        add     a, '0'
        cp      '9' + 1
        jr      c, 2f
        add     a, 'A' - '9' - 1
2:      ld      e, a
        ld      c, CONOUT
        jp      BDOS

msg_title:      .ascii  "Eureka ROM dump\r\n$"
msg_not_eureka: .ascii  "This is not a Eureka A4\r\n$"
msg_standard:   .ascii  "Standard Eureka\r\n$"
msg_advanced:   .ascii  "Advanced English Eureka\r\n$"
msg_reading:    .ascii  "Reading, please wait\r\n$"
msg_bank:       .ascii  "bank 0 $"
msg_data:       .ascii  "data\r\n$"
msg_empty:      .ascii  "empty\r\n$"
msg_unstable:   .ascii  "unstable\r\n$"
msg_same:       .ascii  "same as bank 0\r\n$"
msg_writing512: .ascii  "Writing A4ROM.DMP\r\n$"
msg_layout256:  .ascii  "ROM on 27C256 chips, not yet run by the emulator\r\n"
                .ascii  "Writing A4ROM256.DMP\r\n$"
msg_unknown:    .ascii  "Unknown ROM layout, nothing written\r\n$"
msg_bad_map:    .ascii  "Unexpected memory map, nothing written\r\n$"
msg_cbar:       .ascii  "CBAR $"
msg_bbr:        .ascii  " BBR $"
msg_cbr:        .ascii  " CBR $"
msg_crlf:       .ascii  "\r\n$"
msg_no_create:  .ascii  "Unable to create file\r\n$"
msg_write_error: .ascii "Write error, disk full?\r\n$"
msg_done:       .ascii  "Done\r\n$"
name512:        .ascii  "A4ROM   DMP"
name256:        .ascii  "A4ROM256DMP"

v_cbar:         .byte   0
v_bbr:          .byte   0
v_cbr:          .byte   0
v_dar0h:        .byte   0
v_dar0b:        .byte   0
v_first:        .byte   0
v_varied_page:  .byte   0               ; must follow v_first (IY+1)
v_varied:       .byte   0
v_unstable:     .byte   0
v_sigbase:      .word   0
v_kind:         .byte   0
v_mirror:       .byte   0
v_nbanks:       .byte   0
v_data:         .byte   0, 0, 0, 0, 0
fcb:            .byte   0               ; default drive
                .fill   35, 1, 0
end:
