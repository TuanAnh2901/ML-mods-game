import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;
import ghidra.program.model.symbol.SymbolTable;
import ghidra.program.model.symbol.RefType;

public class MetadataTriage extends GhidraScript {
    private static final String HEADER_GLOBAL = "185f2e9f8";

    private boolean targetsHeaderGlobal(Instruction instruction, Address headerGlobal) {
        for (int operand = 0; operand < instruction.getNumOperands(); operand++) {
            boolean ripRelative = false;
            Scalar displacement = null;
            for (Object object : instruction.getOpObjects(operand)) {
                if (object instanceof Address && object.equals(headerGlobal)) {
                    return true;
                }
                if (object instanceof Register && ((Register) object).getBaseRegister().getName().equals("RIP")) {
                    ripRelative = true;
                }
                if (object instanceof Scalar) {
                    displacement = (Scalar) object;
                }
            }
            if (ripRelative && displacement != null) {
                Address target = instruction.getAddress().add(instruction.getLength())
                    .add(displacement.getSignedValue());
                if (target.equals(headerGlobal)) {
                    return true;
                }
            }
        }
        return false;
    }

    private void emitWindow(String addressText, Listing listing) throws Exception {
        Address address = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(addressText);
        Instruction instruction = listing.getInstructionAt(address);
        println("WINDOW " + addressText);
        if (instruction == null) {
            println("  no decoded instruction");
            return;
        }
        Instruction start = instruction;
        for (int i = 0; i < 24 && start.getPrevious() != null; i++) {
            start = start.getPrevious();
        }
        for (int i = 0; i < 48 && start != null; i++) {
            println("  " + start.getAddress() + "  " + start);
            start = start.getNext();
        }
    }

    private void emitCallers(Symbol symbol, FunctionManager functions) {
        println("IMPORT " + symbol.getName() + " @ " + symbol.getAddress());
        for (Reference reference : getReferencesTo(symbol.getAddress())) {
            Function caller = functions.getFunctionContaining(reference.getFromAddress());
            println("  caller=" + (caller == null ? "<none>" : caller.getName() + " @ " + caller.getEntryPoint())
                + " callsite=" + reference.getFromAddress());
            if (caller != null) {
                continue;
            }
            for (Reference iatReference : getReferencesTo(reference.getFromAddress())) {
                Function iatCaller = functions.getFunctionContaining(iatReference.getFromAddress());
                println("    IAT caller=" + (iatCaller == null ? "<none>" : iatCaller.getName() + " @ " + iatCaller.getEntryPoint())
                    + " callsite=" + iatReference.getFromAddress());
            }
        }
    }

    @Override
    public void run() throws Exception {
        FunctionManager functions = currentProgram.getFunctionManager();
        SymbolTable symbols = currentProgram.getSymbolTable();
        Listing listing = currentProgram.getListing();

        println("=== CUSTOM HEADER GLOBAL RIP-RELATIVE REFERENCES ===");
        Address headerGlobal = currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(HEADER_GLOBAL);
        InstructionIterator headerInstructions = listing.getInstructions(true);
        int headerReferenceCount = 0;
        while (headerInstructions.hasNext()) {
            if (monitor.isCancelled()) {
                break;
            }
            Instruction instruction = headerInstructions.next();
            if (targetsHeaderGlobal(instruction, headerGlobal)) {
                headerReferenceCount++;
                Function headerFunction = functions.getFunctionContaining(instruction.getAddress());
                println("HEADER_REF " + headerReferenceCount + " " + instruction.getAddress() + " " + instruction
                    + " FUNC " + (headerFunction == null ? "<none>" : headerFunction.getName() + " @ " + headerFunction.getEntryPoint()));
                Instruction current = instruction;
                for (int i = 0; i < 20 && current != null; i++) {
                    println("  HEADER_WINDOW " + current.getAddress() + " " + current);
                    current = current.getNext();
                }
            }
        }
        println("HEADER_REF_COUNT " + headerReferenceCount);

        println("PROGRAM " + currentProgram.getName());
        println("IMAGE_BASE " + currentProgram.getImageBase());
        println("=== EXPORTED il2cpp_init ===");
        SymbolIterator initSymbols = symbols.getSymbols("il2cpp_init");
        while (initSymbols.hasNext()) {
            Symbol symbol = initSymbols.next();
            Function init = functions.getFunctionAt(symbol.getAddress());
            if (init == null) {
                init = functions.getFunctionContaining(symbol.getAddress());
            }
            println("SYMBOL " + symbol.getName() + " @ " + symbol.getAddress());
            if (init == null) {
                println("  no function at symbol");
                continue;
            }
            for (Instruction instruction : listing.getInstructions(init.getBody(), true)) {
                if (!instruction.getFlowType().isCall()) {
                    continue;
                }
                for (Reference reference : instruction.getReferencesFrom()) {
                    if (reference.getReferenceType().isCall()) {
                        Function callee = functions.getFunctionAt(reference.getToAddress());
                        println("  call " + instruction.getAddress() + " -> "
                            + (callee == null ? reference.getToAddress() : callee.getName() + " @ " + callee.getEntryPoint()));
                    }
                }
            }
        }

        println("=== il2cpp_init WINDOW ===");
        emitWindow("1804473d0", listing);

        println("=== FILE-I/O IMPORT CALLERS ===");
        SymbolIterator externals = symbols.getExternalSymbols();
        while (externals.hasNext()) {
            Symbol symbol = externals.next();
            String name = symbol.getName().toLowerCase();
            if (name.contains("readfile") || name.contains("createfile") || name.equals("fread") || name.equals("open") || name.contains("mapview")) {
                emitCallers(symbol, functions);
            }
        }

        println("=== ReadFile CALLSITE WINDOWS ===");
        emitWindow("1805212cb", listing);
        emitWindow("180521f47", listing);
        emitWindow("1805217a4", listing);
        emitWindow("18052194e", listing);
        emitWindow("1804a5ddc", listing);

        println("=== METADATA-LIKE STRINGS ===");
        for (ghidra.program.model.listing.Data data : listing.getDefinedData(true)) {
            Object value = data.getValue();
            if (!(value instanceof String)) {
                continue;
            }
            String text = ((String) value).toLowerCase();
            if (text.contains("metadata") || text.contains("il2cpp_data") || text.contains("wasting_your_life")) {
                println("STRING " + data.getAddress() + " " + value);
            }
        }
    }
}
