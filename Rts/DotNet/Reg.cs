using System;
using System.Collections;
using Scheme.Rep;

namespace Scheme.RT {

public class Reg {

    public static readonly int NREGS = 32;
    public static readonly int LASTREG = 31;

    // General-Purpose Registers

    public static SObject register0;
    public static SObject register1;
    public static SObject register2;
    public static SObject register3;
    public static SObject register4;
    public static SObject register5;
    public static SObject register6;
    public static SObject register7;
    public static SObject register8;
    public static SObject register9;
    public static SObject register10;
    public static SObject register11;
    public static SObject register12;
    public static SObject register13;
    public static SObject register14;
    public static SObject register15;
    public static SObject register16;
    public static SObject register17;
    public static SObject register18;
    public static SObject register19;
    public static SObject register20;
    public static SObject register21;
    public static SObject register22;
    public static SObject register23;
    public static SObject register24;
    public static SObject register25;
    public static SObject register26;
    public static SObject register27;
    public static SObject register28;
    public static SObject register29;
    public static SObject register30;
    public static SObject register31;

    public static SObject Result;
    public static SObject Second;
    public static SObject Third;

    // Debugging
    // debugLocation: corresponds to line# in listing file
    public static int debugLocation;
    // debugFile: listing file current code
    public static string debugFile;
    
    // Timer Interrupts
    public static readonly int TIME_SLICE = 8000;
    public static readonly int SMALL_TIME_SLICE = 10;

    public static int timer = TIME_SLICE;
    public static bool interruptsEnabled = false;

    // Assembly used to find compiled-in codevectors
    public static System.Reflection.Assembly programAssembly = null;

    // Implicit Continuation for Special Operations
    /* Jump index of implicit continuation in current procedure. Used by 
     * (for example) arithmetic operations which may cause recoverable
     * exceptions. Set from call site, should be read by operation fault 
     * handler. Not used by timer interrupts.
     */
    public static int implicitContinuation = -1;

    // Globals
    public static Hashtable globals = new Hashtable();

    // Interned Symbols
    // Used until (go ...) replaces with oblist.sch implementation
    public static Hashtable internedSymbols = new Hashtable();

    /* ===================================================== */
    /*   Support Code                                        */
    /* ===================================================== */

    static Reg() {
        Timer.mainTimer.reset();
    }

    public static SPair globalCell(string identifier) {
        SPair cell = (SPair) globals[identifier];
        if (cell == null) {
            cell = new SPair(SObject.Undefined,
                             Factory.makeSymbol(identifier));
            globals[identifier] = cell;
        }
        return cell;
    }

    public static SObject globalValue(string identifier) {
        SPair cell = globalCell(identifier);
        return cell.first;
    }

    public static SObject getRegister(int index) {
        switch (index) {
        case 0: return Reg.register0;
        case 1: return Reg.register1;
        case 2: return Reg.register2;
        case 3: return Reg.register3;
        case 4: return Reg.register4;
        case 5: return Reg.register5;
        case 6: return Reg.register6;
        case 7: return Reg.register7;
        case 8: return Reg.register8;
        case 9: return Reg.register9;
        case 10: return Reg.register10;
        case 11: return Reg.register11;
        case 12: return Reg.register12;
        case 13: return Reg.register13;
        case 14: return Reg.register14;
        case 15: return Reg.register15;
        case 16: return Reg.register16;
        case 17: return Reg.register17;
        case 18: return Reg.register18;
        case 19: return Reg.register19;
        case 20: return Reg.register20;
        case 21: return Reg.register21;
        case 22: return Reg.register22;
        case 23: return Reg.register23;
        case 24: return Reg.register24;
        case 25: return Reg.register25;
        case 26: return Reg.register26;
        case 27: return Reg.register27;
        case 28: return Reg.register28;
        case 29: return Reg.register29;
        case 30: return Reg.register30;
        case 31: return Reg.register31;
        default:
            Exn.internalError("getRegister: register index out of bounds: " + index);
            return null;
        }
    }

    public static void setRegister(int index, SObject value) {
        switch (index) {
        case 0: Reg.register0 = value; break;
        case 1: Reg.register1 = value; break;
        case 2: Reg.register2 = value; break;
        case 3: Reg.register3 = value; break;
        case 4: Reg.register4 = value; break;
        case 5: Reg.register5 = value; break;
        case 6: Reg.register6 = value; break;
        case 7: Reg.register7 = value; break;
        case 8: Reg.register8 = value; break;
        case 9: Reg.register9 = value; break;
        case 10: Reg.register10 = value; break;
        case 11: Reg.register11 = value; break;
        case 12: Reg.register12 = value; break;
        case 13: Reg.register13 = value; break;
        case 14: Reg.register14 = value; break;
        case 15: Reg.register15 = value; break;
        case 16: Reg.register16 = value; break;
        case 17: Reg.register17 = value; break;
        case 18: Reg.register18 = value; break;
        case 19: Reg.register19 = value; break;
        case 20: Reg.register20 = value; break;
        case 21: Reg.register21 = value; break;
        case 22: Reg.register22 = value; break;
        case 23: Reg.register23 = value; break;
        case 24: Reg.register24 = value; break;
        case 25: Reg.register25 = value; break;
        case 26: Reg.register26 = value; break;
        case 27: Reg.register27 = value; break;
        case 28: Reg.register28 = value; break;
        case 29: Reg.register29 = value; break;
        case 30: Reg.register30 = value; break;
        case 31: Reg.register31 = value; break;
        default:
            Exn.internalError("setRegister: register index out of bounds: " + index);
            return;
        }
    }
    
    public static void clearRegisters() {
        Reg.Result = SObject.Undefined;
        Reg.Second = SObject.Undefined;
        Reg.Third = SObject.Undefined;
        for (int i = 0; i < Reg.NREGS; ++i) {
            Reg.setRegister(i, SObject.Undefined);
        }
    }
}
}