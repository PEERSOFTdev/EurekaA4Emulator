#!/usr/bin/env python3
"""Build EUTEST13.COM for Eureka A4. No third-party assembler required.
Pure byte assembler, checked label fixups, 8.3 filename, finite CPU loops.
This program does not modify the emulator, ROM, MMU, or timer configuration.
"""
from pathlib import Path
import argparse, json, hashlib

class Assembler:
    def __init__(self):
        self.data=bytearray(); self.labels={}; self.fixups=[]; self.relatives=[]
    @property
    def pc(self): return 0x100+len(self.data)
    def label(self,name):
        if name in self.labels: raise ValueError("Duplicate label "+name)
        self.labels[name]=self.pc
    def emit(self,*values):
        if any(not 0<=x<=255 for x in values): raise ValueError(values)
        self.data.extend(values)
    def word(self,value):
        if isinstance(value,str):
            self.fixups.append((len(self.data),value)); self.emit(0,0)
        else: self.emit(value&255,(value>>8)&255)
    def absop(self,opcode,label):
        self.emit(opcode);self.word(label)
    def edabs(self,opcode,label):
        self.emit(0xed,opcode);self.word(label)
    def call(self,label): self.absop(0xcd,label)
    def jp(self,label,opcode=0xc3):self.absop(opcode,label)
    def jr(self,label,opcode=0x20):
        self.emit(opcode);self.relatives.append((len(self.data),label));self.emit(0)
    def de(self,label): self.absop(0x11,label)
    def hl(self,label): self.absop(0x21,label)
    def ld_a(self,label):self.absop(0x3a,label)
    def st_a(self,label):self.absop(0x32,label)
    def bytes(self,b):self.data.extend(b)
    def finish(self):
        for index,label in self.fixups:
            value=self.labels[label]
            self.data[index:index+2]=value.to_bytes(2,'little')
        for index,label in self.relatives:
            delta=self.labels[label]-(0x100+index+1)
            if not -128<=delta<=127:raise ValueError((label,delta))
            self.data[index]=delta&255
        if self.pc>=0xb000:raise ValueError("Program exceeds conservative TPA limit")
        return bytes(self.data)

def build(outdir):
    a=Assembler()
    phases=[("BASE",56,60,255)]
    for tag,p50,p54,p62 in [
        ("50_96",96,60,255),("96_62",96,60,1),
        ("50_170",170,60,255),("170_62",170,60,1),
        ("54_252",56,252,255)]:
        phases.append((tag,p50,p54,p62));phases.append(("RESTORE",56,60,255))
    records=[]; strings=[]; times={}
    def record(label,text):
        if len(text.encode('ascii'))>126:raise ValueError(text)
        records.append((label,text))
    def string(label,text): strings.append((label,text+"\r\n$"))
    def check_ff(error):a.emit(0xfe,0xff);a.jp(error,0xca)
    def check_nz(error):a.emit(0xb7);a.jp(error,0xc2)
    def bdos(fn):
        a.emit(0x0e,fn);a.call("bdos")
    def file_call(fn):a.de("fcb");bdos(fn)
    def port_read(port):
        a.emit(0x01,port,0,0xed,0x78) # address BC=00pp
    def port_write(port,value):
        a.emit(0x01,port,0,0x3e,value,0xed,0x79)
    def compare_baseline(error):
        # Only DIM1 (bit 1, channel-1 transfer direction) may differ after disk I/O.
        # This accepts exactly 38h/3Ah, NOT arbitrary low nibbles or changed WAITs.
        for port,want in [(50,56),(54,60),(62,255),(63,31)]:
            port_read(port)
            if port==50:a.emit(0xe6,0xfd) # AND FDh: preserve every other bit
            a.emit(0xfe,want);a.jp(error,0xc2)
    def append(label,count=1):
        a.de(label);a.emit(0x3e,count);a.call("append_block")
    def print_s(label):a.de(label);a.call("print")

    # Own reserved stack; no firmware-stack writes. Exit via BDOS 0.
    a.absop(0x31,"stack_top")
    a.emit(0xaf);a.st_a("changed")
    a.st_a("initial_mismatch")
    a.emit(0x3e,255);a.st_a("phase_id")
    # Capture all four registers BEFORE any BDOS/console/file operation.
    # A bad initial state is never silently reset or accepted for timing tests.
    a.call("snapshot_initial")
    print_s("intro")
    a.call("clear_fcb");file_call(15)
    a.emit(0xfe,0xff);a.jp("err_exists",0xc2)
    a.call("clear_fcb");file_call(22);check_ff("err_make")
    a.de("header0");a.call("write_record")
    a.de("header1");a.call("write_record")
    a.de("header2");a.call("write_record")
    a.de("initial_hex");a.call("write_record")
    a.de("initial_dec");a.call("write_record")
    a.de("initial_check");a.call("write_record")
    a.ld_a("initial_mismatch");a.emit(0xb7);a.jp("initial_bad_saved",0xc2)
    a.de("initial_ok");a.call("write_record")
    a.call("close_file")
    print_s("initial_saved_msg")
    # Check again after file I/O: no test is allowed to hide a state change.
    a.label("initial_after_save_check");compare_baseline("err_pretest")
    a.jp("phases_start")
    a.label("initial_bad_saved")
    a.de("initial_abort");a.call("write_record")
    a.call("close_file")
    a.ld_a("initial_actual");a.jp("err_initial")
    a.label("phases_start")

    for idx,(name,p50,p54,p62) in enumerate(phases):
        prefix=f"p{idx:02}"
        record(prefix+"_begin",
               f"BEGIN P{idx:02} {name} REQUEST DEC P50={str(p50) if p50 != 56 else 'SAVED'} P54={p54} P62={p62}")
        string(prefix+"_msg",f"EUTEST13: {idx+1}/11 {name}")
        print_s(prefix+"_msg")
        append(prefix+"_begin")  # durable checkpoint before any register change
        a.emit(0x3e,idx);a.st_a("phase_id")
        a.emit(0x3e,1);a.st_a("changed")
        # Each phase starts from documented user-verified settings.
        a.call("restore")
        # restore sets changed to zero only on error-free return; mark again:
        a.emit(0x3e,1);a.st_a("changed")
        if p50!=56:port_write(50,p50)
        if p54!=60:port_write(54,p54)
        if p62!=255:port_write(62,p62) # deliberate order: 50 first, then 62
        a.label(prefix+"_active")
        a.hl(prefix+"_state");a.call("capture_ports")
        for kind in ("CPU","READ","WRITE"):
            a.hl(prefix+"_"+kind.lower());a.call("measure_"+kind.lower())
        a.call("restore")
        a.label(prefix+"_restored")
        a.hl(prefix+"_restore");a.call("capture_ports")
        compare_baseline("err_restore")
        append(prefix+"_state",5)  # modes + 3 timing records + restoration
        record(prefix+"_state",f"STATE P{idx:02} P50=XX P54=XX P62=XX P63=XX")
        for kind in ("CPU","READ","WRITE"):
            # all fields have common offsets; time is HH:MM:SS:CS, binary hex
            record(prefix+"_"+kind.lower(),
                   f"TIME P{idx:02} {kind:<5} START=XX:XX:XX:XX END=XX:XX:XX:XX LOOP=16x65536 HEX-BINARY HH:MM:SS:CS")
        record(prefix+"_restore",f"RESTO P{idx:02} P50=XX P54=XX P62=XX P63=XX")

    append("done_record")
    # Closing the log can switch DMA direction; return the entry settings once more.
    a.call("restore");compare_baseline("err_restore_unresolved")
    print_s("done_msg");a.jp("exit")

    # Raw values are saved before formatting. Each field is independent of
    # registers overwritten by hex/decimal conversion (EUTEST4 regression).
    a.label("snapshot_initial")
    for p in (50,54,62,63):
        port_read(p);a.st_a("initial"+str(p))
    for p,want,bit in [(50,56,1),(54,60,2),(62,255,4),(63,31,8)]:
        a.ld_a("initial"+str(p))
        if p==50:a.emit(0xe6,0xfd)
        a.emit(0xfe,want)
        a.jp("initial_match_"+str(p),0xca)
        # Record the first failing port/value as well as a mask of ALL differences.
        a.ld_a("initial_mismatch");a.emit(0xb7)
        a.jp("initial_more_"+str(p),0xc2)
        a.emit(0x3e,p);a.st_a("initial_port")
        a.ld_a("initial"+str(p));a.st_a("initial_actual")
        a.label("initial_more_"+str(p))
        a.ld_a("initial_mismatch");a.emit(0xf6,bit);a.st_a("initial_mismatch")
        a.label("initial_match_"+str(p))
    for p in (50,54,62,63):
        a.ld_a("initial"+str(p));a.hl("initial_hex")
        a.de("INITIAL HEX P50=XX P54=XX P62=XX P63=XX".index("XX",("INITIAL HEX P50=XX P54=XX P62=XX P63=XX").index("P"+str(p))))
        a.emit(0x19);a.call("hex2")
        a.ld_a("initial"+str(p));a.hl("initial_dec")
        a.de("INITIAL DEC P50=DDD P54=DDD P62=DDD P63=DDD".index("DDD",("INITIAL DEC P50=DDD P54=DDD P62=DDD P63=DDD").index("P"+str(p))))
        a.emit(0x19);a.call("decimal3")
    a.ld_a("initial_mismatch");a.hl("initial_check");a.de(19);a.emit(0x19);a.call("hex2")
    a.emit(0xc9)

    # A -> three decimal digits at HL; no BDOS and no hardware writes.
    a.label("decimal3");a.emit(0x06,48)
    a.label("hundreds");a.emit(0xfe,100);a.jp("tens_start",0xda)
    a.emit(0xd6,100,0x04);a.jp("hundreds")
    a.label("tens_start");a.emit(0x70,0x23,0x06,48)
    a.label("tens");a.emit(0xfe,10);a.jp("ones",0xda)
    a.emit(0xd6,10,0x04);a.jp("tens")
    a.label("ones");a.emit(0x70,0x23,0xc6,48,0x77,0xc9)

    # Restore the entry DCNTL value (38h or 3Ah), not an invented fixed DMA direction.
    # In both cases WAIT bits are identical. Disk ROM may later change DIM1 again.
    # No file/console calls and no DMA enable/disable writes here.
    a.label("restore")
    a.emit(0x01,50,0);a.ld_a("initial50")
    a.label("restore_p50_out");a.emit(0xed,0x79)
    for port,val in [(54,60),(62,255)]:port_write(port,val)
    a.emit(0xaf);a.st_a("changed");a.emit(0xc9)

    a.label("capture_ports");a.absop(0x22,"text_ptr")
    port_template="STATE P00 P50=XX P54=XX P62=XX P63=XX"
    for port in (50,54,62,63):
        port_read(port)
        a.absop(0x2a,"text_ptr")
        a.de(port_template.index("XX",port_template.index("P"+str(port))))
        a.emit(0x19);a.call("hex2")
    a.emit(0xc9)

    # Snapshot reads 0090h first to latch the remaining calendar fields.
    # All four bytes are saved before formatting or calling any OS function.
    a.label("rtc_snapshot")
    for port,label in [(0x90,"rtc_cs"),(0x91,"rtc_h"),(0x92,"rtc_m"),(0x93,"rtc_s")]:
        port_read(port);a.st_a(label)
    a.emit(0xc9)

    time_template="TIME P00 CPU   START=XX:XX:XX:XX END=XX:XX:XX:XX LOOP=16x65536 HEX-BINARY HH:MM:SS:CS"
    for kind in ("cpu","read","write"):
        a.label("measure_"+kind);a.absop(0x22,"text_ptr")
        a.call("rtc_snapshot")
        # Save start; formatting is postponed until after the timed body.
        for src,dst in [("rtc_h","start_h"),("rtc_m","start_m"),("rtc_s","start_s"),("rtc_cs","start_cs")]:
            a.ld_a(src);a.st_a(dst)
        a.emit(0x3e,16);a.st_a("outer")
        a.label("body_"+kind);a.label("outer_"+kind);a.emit(0x01,0,0)
        if kind!="cpu":a.hl("test_byte")
        a.label("inner_"+kind)
        if kind=="cpu":a.emit(0,0)
        elif kind=="read":a.emit(0x7e,0x7e)
        else:a.emit(0x77,0x77)
        a.emit(0x0b,0x78,0xb1);a.jr("inner_"+kind)
        a.ld_a("outer");a.emit(0x3d);a.st_a("outer");a.jr("outer_"+kind)
        a.label("body_end_"+kind)
        a.call("rtc_snapshot")
        # Markers below bound full measurement including the two RTC snapshots.
        for field,labels in [
            ("START=",("start_h","start_m","start_s","start_cs")),
            ("END=",("rtc_h","rtc_m","rtc_s","rtc_cs"))]:
            off=time_template.index(field)+len(field)
            for i,label in enumerate(labels):
                a.ld_a(label);a.absop(0x2a,"text_ptr");a.de(off+3*i);a.emit(0x19);a.call("hex2")
        a.label("measure_end_"+kind);a.emit(0xc9)
        times[kind]={"entry":"measure_"+kind,"exit":"measure_end_"+kind,
                     "body":"body_"+kind,"body_end":"body_end_"+kind,"inner":"inner_"+kind}

    a.label("hex2")
    a.emit(0xf5,0x0f,0x0f,0x0f,0x0f,0xe6,15);a.call("nibble")
    a.emit(0x77,0x23,0xf1,0xe6,15);a.call("nibble");a.emit(0x77,0xc9)
    a.label("nibble")
    a.emit(0xc6,48,0xfe,58);a.jp("nibble_end",0xda);a.emit(0xc6,7)
    a.label("nibble_end");a.emit(0xc9)

    # DE=first record, A=count. Append by sequential read to EOF, not random IO.
    a.label("append_block")
    a.edabs(0x53,"records_ptr");a.st_a("records_count")
    a.call("clear_fcb");file_call(15);check_ff("err_open")
    a.de("scratch");bdos(26)
    a.emit(0x06,128)
    a.label("scan")
    a.emit(0xc5);file_call(20);a.emit(0xc1)
    a.emit(0xfe,1);a.jp("append_records",0xca) # only return 1 is accepted as EOF
    a.emit(0xb7);a.jp("err_read",0xc2)
    a.jr("scan",0x10);a.jp("err_limit")
    a.label("append_records")
    a.edabs(0x5b,"records_ptr");a.call("write_record")
    a.absop(0x2a,"records_ptr");a.de(128);a.emit(0x19);a.absop(0x22,"records_ptr")
    a.ld_a("records_count");a.emit(0x3d);a.st_a("records_count");a.jr("append_records")
    a.call("close_file");a.emit(0xc9)

    a.label("clear_fcb")
    a.hl("fcb_tail");a.emit(0x06,24,0xaf)
    a.label("zero_fcb");a.emit(0x77,0x23);a.jr("zero_fcb",0x10);a.emit(0xc9)
    a.label("write_record")
    bdos(26);file_call(21);check_nz("err_write");a.emit(0xc9)
    a.label("close_file");file_call(16);check_ff("err_close");a.emit(0xc9)
    a.label("print");bdos(9);a.emit(0xc9)
    a.label("bdos");a.emit(0xcd,5,0,0xc9)

    # Initial mismatch has already been written and CLOSED before reaching here.
    a.label("err_initial");a.st_a("error_code")
    print_s("err_initial_msg");a.jp("exit")
    string("err_initial_msg","ERROR INITIAL STATE - DETAILS SAVED IN EUTEST13.TXT. NO TEST OUT EXECUTED.")

    # If disk/console changed the initial state, preserve the reason and stop.
    # No application OUT is issued on this branch either.
    a.label("err_pretest")
    a.hl("error_state");a.call("capture_error_ports")
    append("pretest_error");append("error_state")
    print_s("err_pretest_msg");a.jp("exit")
    string("err_pretest_msg","ERROR PRETEST STATE - SEE EUTEST13.TXT. NO TEST RUN.")

    # If a restoration check fails, keep the observed values before a single
    # bounded retry. Only append the error after readback is verified again.
    a.label("err_restore")
    a.hl("error_state");a.call("capture_error_ports")
    a.call("restore")
    a.label("restore_retry_check");compare_baseline("err_restore_unresolved")
    append("restore_error");append("error_state")
    print_s("err_restore_msg");a.jp("exit")
    string("err_restore_msg","ERROR RESTORE - SAVED IN EUTEST13.TXT. RETRY RESTORED BASELINE; TEST STOPPED.")
    a.label("err_restore_unresolved")
    print_s("err_restore_unresolved_msg");a.jp("exit")
    string("err_restore_unresolved_msg","ERROR RESTORE PERSISTS. NO FURTHER DISK WRITE. SEND PARTIAL EUTEST13.TXT.")

    a.label("capture_error_ports")
    a.call("capture_ports")
    a.ld_a("phase_id");a.hl("error_state");a.de(7);a.emit(0x19);a.call("hex2")
    a.emit(0xc9)

    errors=[
        ("err_exists","EUTEST13.TXT EXISTS - NOT OVERWRITTEN; MOVE IT BEFORE NEXT RUN"),
        ("err_make","ERROR MAKE BDOS22"),("err_open","ERROR OPEN BDOS15"),
        ("err_read","ERROR READ BDOS20 - NOT EOF"),
        ("err_limit","ERROR APPEND LIMIT"),("err_write","ERROR WRITE BDOS21"),
        ("err_close","ERROR CLOSE BDOS16") ]
    for label,text in errors:
        a.label(label);a.st_a("error_code")
        # Do not recursively try to write an error through the failing disk path.
        # Previously CLOSED checkpoints remain; the current write is not guaranteed.
        a.ld_a("changed");a.emit(0xb7);a.call("restore_if_changed")
        a.hl("err_raw_field");a.ld_a("error_code");a.call("hex2")
        print_s(label+"_msg");print_s("err_raw");a.jp("exit")
        string(label+"_msg",text)
    a.label("restore_if_changed");a.emit(0xc8);a.jp("restore") # RET Z or tail-call restore
    a.label("exit");a.emit(0x0e,0);a.call("bdos");a.emit(0xc3,0,0)

    string("intro","EUTEST13 - PORT50 96/170, OMCR COMBINATIONS, PORT54 252. EXPERIMENTAL.")
    string("initial_saved_msg","INITIAL STATE SAVED IN EUTEST13.TXT.")
    string("done_msg","TEST COMPLETE - EUTEST13.TXT - NORMAL PORT SETTINGS RESTORED")
    for name,text in strings:a.label(name);a.bytes(text.encode("ascii"))
    a.label("err_raw");a.bytes(b"RAW RETURN A=");a.label("err_raw_field");a.bytes(b"XX\r\n$")
    a.label("fcb");a.emit(0);a.bytes(b"EUTEST13TXT");a.label("fcb_tail");a.bytes(bytes(24))
    record("header0","EUREKA A4 EUTEST13 V1 - CONTROLLED OUT50 / OUT54 / OMCR TEST")
    record("header1","ACCEPT DEC P50=56 OR 58; P54=60 P62=255 P63=31. SAME WAIT BITS; RETURN P50=SAVED ENTRY. NOT HARDWARE RESET.")
    record("header2","TIMES ARE RAW BINARY IN HEX: HOURS:MINUTES:SECONDS:HUNDREDTHS. INTERRUPTS REMAIN ENABLED.")
    record("initial_hex","INITIAL HEX P50=XX P54=XX P62=XX P63=XX")
    record("initial_dec","INITIAL DEC P50=DDD P54=DDD P62=DDD P63=DDD")
    record("initial_check","INITIAL DIFFERENCE=XX; REJECT BITS 01=P50 02=P54 04=P62 08=P63; P50 DIM1 IGNORED; 00=ACCEPTED")
    record("initial_ok","INITIAL STATE ACCEPTED (P50=38h OR 3Ah). SAVED P50 IS RESTORED; EXACT TEST WRITES REMAIN 96/170. 11 PHASES.")
    record("initial_abort","ERROR INITIAL STATE. ABORTED BEFORE TESTS. NO APPLICATION OUT TO P50/P54/P62. VALUES ABOVE ARE UNCHANGED SNAPSHOT.")
    record("pretest_error","ERROR PRETEST STATE AFTER INITIAL SAVE. NO APPLICATION OUT. TEST ABORTED; OBSERVED STATE FOLLOWS.")
    record("restore_error","ERROR RESTORE READBACK. OBSERVED FAILED STATE FOLLOWS; ONE RETRY RESTORED BASELINE. TEST ABORTED.")
    record("error_state","ERROR PXX P50=XX P54=XX P62=XX P63=XX")
    record("done_record","TEST COMPLETE EUTEST13 V1 - ALL PHASES SAVED; FINAL BASELINE VERIFIED.")
    for name,text in records:
        a.label(name);a.bytes(text.encode("ascii").ljust(126,b" ")+b"\r\n")
    for name in ("initial50","initial54","initial62","initial63","initial_mismatch","initial_port","initial_actual","changed","phase_id","outer","test_byte","records_count","error_code",
                 "rtc_h","rtc_m","rtc_s","rtc_cs","start_h","start_m","start_s","start_cs"):
        a.label(name);a.emit(0x5a if name=="test_byte" else 0)
    for name in ("text_ptr","records_ptr"):a.label(name);a.emit(0,0)
    a.label("scratch");a.bytes(bytes(128))
    a.label("stack_guard");a.bytes(bytes([0xa5])*16)
    a.label("stack");a.bytes(bytes(768));a.label("stack_top")
    binary=a.finish()
    # Record blocks must be consecutive, and filename must be exactly 8+3.
    for idx in range(len(phases)):
        names=[f"p{idx:02}_state"]+[f"p{idx:02}_{k}" for k in ("cpu","read","write")]+[f"p{idx:02}_restore"]
        assert all(a.labels[y]-a.labels[x]==128 for x,y in zip(names,names[1:]))
    fcb=a.labels["fcb"]-0x100
    assert binary[fcb:fcb+12]==b"\0EUTEST13TXT"
    outdir.mkdir(parents=True,exist_ok=True)
    (outdir/"EUTEST13.COM").write_bytes(binary)
    manifest={
        "program":"EUTEST13","build":"V1-DMA-direction-aware","origin":256,"size":len(binary),
        "sha256":hashlib.sha256(binary).hexdigest(),
        "phases":[{"id":i,"name":p[0],"p50":p[1],"p54":p[2],"p62":p[3]} for i,p in enumerate(phases)],
        "loops":16*65536,"expected_records":7+len(phases)*6+1,"initial_error_records":7,"accepted_initial_p50":[56,58],"p50_comparison_mask":253,"baseline_p50":"saved_entry",
        "labels":a.labels,
        "timing":{k:{j:a.labels[label] for j,label in v.items()} for k,v in times.items()},
        "record_texts":dict(records)
    }
    (outdir/"EUTEST13-map.json").write_text(json.dumps(manifest,indent=2)+"\n")
    return manifest

if __name__=="__main__":
    ap=argparse.ArgumentParser();ap.add_argument("--out",type=Path,default=Path(__file__).resolve().parent)
    args=ap.parse_args();m=build(args.out)
    print("Built",m["size"],"bytes; records",m["expected_records"],"SHA256",m["sha256"])
