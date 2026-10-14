// Fixes what auto-analysis gets wrong in a Microsoft C Win16 (NE) program.
// Run as a headless -postScript, after PrepareWin16.
// @category Win16

import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.pcode.JumpTable;
import ghidra.program.model.symbol.RefType;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.SourceType;

public class FixupWin16 extends GhidraScript {

	private static final Set<String> WINDOW_PROCS = Set.of("MAINWNDPROC", "SPLASHWINDOWPROC");
	private static final Set<String> DIALOG_PROCS = Set.of("OPTIONSDLGPROC",
		"HIGHSCOREDLGPROC", "GETHIGHSCORERDLGPROC", "REGISTER", "SERVANTWAREABOUT");

	private static final List<Win16Api.Parameter> CALLBACK_PARAMETERS = List.of(
		new Win16Api.Parameter("HWND", "hWnd"), new Win16Api.Parameter("UINT", "msg"),
		new Win16Api.Parameter("WPARAM", "wParam"), new Win16Api.Parameter("LPARAM", "lParam"));

	@Override
	protected void run() throws Exception {
		overrideSwitches();
		Win16Api api = new Win16Api(currentProgram);
		for (String name : WINDOW_PROCS) {
			applyCallbackSignature(api, name, "LRESULT");
		}
		for (String name : DIALOG_PROCS) {
			applyCallbackSignature(api, name, "BOOL");
		}
		analyzeChanges(currentProgram);
	}

	private void overrideSwitches() throws Exception {
		for (Win16SwitchTables.SwitchTable sw : Win16SwitchTables.find(currentProgram,
			monitor)) {
			Instruction jump = getInstructionAt(sw.jump());
			Function function = getFunctionContaining(sw.jump());
			if (jump == null || function == null) {
				printerr("Switch at " + sw.jump() + " is not inside a function; skipped");
				continue;
			}

			for (Reference ref : jump.getReferencesFrom()) {
				if (ref.getReferenceType().isComputed()) {
					jump.removeOperandReference(ref.getOperandIndex(), ref.getToAddress());
				}
			}
			ArrayList<Address> destinations = new ArrayList<>(new LinkedHashSet<>(sw.targets()));
			for (Address destination : destinations) {
				disassemble(destination);
				jump.addOperandReference(0, destination, RefType.COMPUTED_JUMP,
					SourceType.USER_DEFINED);
			}
			new JumpTable(sw.jump(), destinations, true, 0).writeOverride(function);
			CreateFunctionCmd.fixupFunctionBody(currentProgram, function, monitor);
			println("Switch at " + sw.jump() + " in " + function.getName() + ": " +
				destinations.size() + " destinations");
		}
	}

	private void applyCallbackSignature(Win16Api api, String name, String returnType)
			throws Exception {
		List<Function> functions = getGlobalFunctions(name);
		if (functions.size() != 1) {
			printerr("Expected one function named " + name + ", found " + functions.size());
			return;
		}
		api.apply(functions.get(0),
			new Win16Api.Prototype(name, returnType, CALLBACK_PARAMETERS, false, false));
		println("Applied FAR PASCAL callback signature to " + name);
	}
}
