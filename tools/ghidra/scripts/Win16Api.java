import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import ghidra.program.model.data.ArrayDataType;
import ghidra.program.model.data.ByteDataType;
import ghidra.program.model.data.CategoryPath;
import ghidra.program.model.data.CharDataType;
import ghidra.program.model.data.DWordDataType;
import ghidra.program.model.data.DataType;
import ghidra.program.model.data.DataTypeConflictHandler;
import ghidra.program.model.data.DataTypeManager;
import ghidra.program.model.data.PointerDataType;
import ghidra.program.model.data.ShortDataType;
import ghidra.program.model.data.SignedDWordDataType;
import ghidra.program.model.data.StructureDataType;
import ghidra.program.model.data.TypedefDataType;
import ghidra.program.model.data.VoidDataType;
import ghidra.program.model.data.WordDataType;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Function.FunctionUpdateType;
import ghidra.program.model.listing.ParameterImpl;
import ghidra.program.model.listing.Program;
import ghidra.program.model.listing.ReturnParameterImpl;
import ghidra.program.model.listing.Variable;
import ghidra.program.model.listing.VariableStorage;
import ghidra.program.model.symbol.SourceType;
import ghidra.util.exception.DuplicateNameException;
import ghidra.util.exception.InvalidInputException;

/**
 * Win16 (Windows 3.1) data types and function prototypes.
 * <p>
 * Ghidra names the Win16 API ordinals but ships no prototypes for them, and
 * its x86-16 compiler spec has no pascal convention. FAR PASCAL is modelled
 * as __stdcall16far, whose callee also pops the arguments, with the
 * parameters reversed, since pascal pushes them left to right.
 */
public final class Win16Api {

	public record Parameter(String type, String name) {
	}

	public record Prototype(String name, String returnType, List<Parameter> parameters,
			boolean cdecl, boolean varArgs) {
	}

	private static final CategoryPath CATEGORY = new CategoryPath("/win16");
	private static final String FAR_PASCAL = "__stdcall16far";
	private static final String FAR_CDECL = "__cdecl16far";
	private static final int FAR_POINTER_SIZE = 4;
	private static final int FAR_RETURN_ADDRESS_SIZE = 4;
	private static final int STACK_SLOT_SIZE = 2;

	// [__cdecl] RETURN Name(PARAMETERS);
	private static final Pattern DECLARATION =
		Pattern.compile("(__cdecl\\s+)?(\\w+)\\s+(\\w+)\\s*\\((.*)\\)\\s*;");
	// TYPE name[COUNT]
	private static final Pattern DECLARATOR = Pattern.compile("(\\w+)\\s+(\\w+)(?:\\[(\\d+)\\])?");

	private final Program program;
	private final DataTypeManager dtm;
	private final Map<String, DataType> types = new HashMap<>();

	public Win16Api(Program program) {
		this.program = program;
		this.dtm = program.getDataTypeManager();
		defineTypes();
	}

	public static List<Prototype> parseHeader(Path header) throws IOException {
		List<Prototype> prototypes = new ArrayList<>();
		List<String> lines = Files.readAllLines(header);
		for (int i = 0; i < lines.size(); i++) {
			String line = lines.get(i).strip();
			if (line.isEmpty() || line.startsWith("//")) {
				continue;
			}
			Matcher declaration = DECLARATION.matcher(line);
			if (!declaration.matches()) {
				throw new IOException(header + ":" + (i + 1) + ": unsupported declaration");
			}
			String parameters = declaration.group(4);
			prototypes.add(new Prototype(declaration.group(3), declaration.group(2),
				parseParameters(parameters), declaration.group(1) != null,
				parameters.strip().endsWith("...")));
		}
		return prototypes;
	}

	private static List<Parameter> parseParameters(String list) {
		List<Parameter> parameters = new ArrayList<>();
		for (String parameter : list.split(",")) {
			String text = parameter.strip();
			if (text.equals("void") || text.equals("...")) {
				continue;
			}
			Matcher declarator = DECLARATOR.matcher(text);
			if (!declarator.matches() || declarator.group(3) != null) {
				throw new IllegalArgumentException("Unsupported parameter: " + text);
			}
			parameters.add(new Parameter(declarator.group(1), declarator.group(2)));
		}
		return parameters;
	}

	public DataType type(String name) {
		DataType type = types.get(name);
		if (type == null) {
			throw new IllegalArgumentException("Unknown Win16 type: " + name);
		}
		return type;
	}

	// Storage is laid out by hand: Ghidra aligns 4-byte parameters to 4 on the
	// stack, but 16-bit code pushes everything on 2-byte boundaries.
	public void apply(Function function, Prototype prototype)
			throws InvalidInputException, DuplicateNameException {
		List<Parameter> stackOrder = new ArrayList<>(prototype.parameters());
		if (!prototype.cdecl()) {
			Collections.reverse(stackOrder);
		}
		List<Variable> parameters = new ArrayList<>();
		int stackOffset = FAR_RETURN_ADDRESS_SIZE;
		for (Parameter parameter : stackOrder) {
			DataType type = type(parameter.type());
			parameters.add(new ParameterImpl(parameter.name(), type, stackOffset, program));
			stackOffset += Math.max(STACK_SLOT_SIZE, type.getLength());
		}

		DataType returnType = type(prototype.returnType());
		Variable returnValue =
			new ReturnParameterImpl(returnType, returnStorage(returnType), program);

		Function target = function.isThunk() ? function.getThunkedFunction(true) : function;
		target.updateFunction(prototype.cdecl() ? FAR_CDECL : FAR_PASCAL, returnValue,
			parameters, FunctionUpdateType.CUSTOM_STORAGE, true, SourceType.USER_DEFINED);
		target.setVarArgs(prototype.varArgs());
	}

	private VariableStorage returnStorage(DataType type) throws InvalidInputException {
		return switch (type.getLength()) {
			case 0 -> VariableStorage.VOID_STORAGE;
			case 1, 2 -> new VariableStorage(program, register("AX"));
			case 4 -> new VariableStorage(program, register("DX"), register("AX"));
			default -> throw new InvalidInputException(
				"No 16-bit return register for " + type.getName());
		};
	}

	private Register register(String name) {
		return program.getLanguage().getRegister(name);
	}

	private void defineTypes() {
		types.put("void", VoidDataType.dataType);
		types.put("char", CharDataType.dataType);
		types.put("int", ShortDataType.dataType);

		typedef("BYTE", ByteDataType.dataType);
		typedef("WORD", WordDataType.dataType);
		typedef("DWORD", DWordDataType.dataType);
		typedef("LONG", SignedDWordDataType.dataType);
		typedef("BOOL", type("int"));
		typedef("HFILE", type("int"));
		typedef("UINT", type("WORD"));
		typedef("ATOM", type("UINT"));
		typedef("WPARAM", type("UINT"));
		typedef("LPARAM", type("LONG"));
		typedef("LRESULT", type("LONG"));
		typedef("COLORREF", type("DWORD"));
		typedef("HANDLE", type("UINT"));
		for (String handle : List.of("HWND", "HDC", "HINSTANCE", "HMENU", "HICON", "HCURSOR",
			"HBRUSH", "HPEN", "HRGN", "HBITMAP", "HGDIOBJ", "HACCEL", "HLOCAL", "HGLOBAL",
			"HTASK")) {
			typedef(handle, type("HANDLE"));
		}

		farPointer("LPSTR", type("char"));
		farPointer("LPCSTR", type("char"));
		farPointer("LPVOID", type("void"));
		// Callback signatures vary, so code pointers stay untyped.
		for (String proc : List.of("FARPROC", "WNDPROC", "DLGPROC", "TIMERPROC")) {
			farPointer(proc, type("void"));
		}

		struct("POINT", "int x", "int y");
		struct("RECT", "int left", "int top", "int right", "int bottom");
		struct("MSG", "HWND hwnd", "UINT message", "WPARAM wParam", "LPARAM lParam",
			"DWORD time", "POINT pt");
		struct("PAINTSTRUCT", "HDC hdc", "BOOL fErase", "RECT rcPaint", "BOOL fRestore",
			"BOOL fIncUpdate", "BYTE rgbReserved[16]");
		struct("WNDCLASS", "UINT style", "WNDPROC lpfnWndProc", "int cbClsExtra",
			"int cbWndExtra", "HINSTANCE hInstance", "HICON hIcon", "HCURSOR hCursor",
			"HBRUSH hbrBackground", "LPCSTR lpszMenuName", "LPCSTR lpszClassName");
		struct("OFSTRUCT", "BYTE cBytes", "BYTE fFixedDisk", "UINT nErrCode",
			"BYTE reserved[4]", "char szPathName[128]");
		struct("BITMAPINFOHEADER", "DWORD biSize", "LONG biWidth", "LONG biHeight",
			"WORD biPlanes", "WORD biBitCount", "DWORD biCompression", "DWORD biSizeImage",
			"LONG biXPelsPerMeter", "LONG biYPelsPerMeter", "DWORD biClrUsed",
			"DWORD biClrImportant");
		struct("RGBQUAD", "BYTE rgbBlue", "BYTE rgbGreen", "BYTE rgbRed", "BYTE rgbReserved");
		struct("BITMAPINFO", "BITMAPINFOHEADER bmiHeader", "RGBQUAD bmiColors[1]");
		for (String struct : List.of("POINT", "RECT", "MSG", "PAINTSTRUCT", "WNDCLASS",
			"OFSTRUCT", "BITMAPINFO")) {
			farPointer("LP" + struct, type(struct));
		}
	}

	private void typedef(String name, DataType base) {
		types.put(name, dtm.addDataType(new TypedefDataType(CATEGORY, name, base, dtm),
			DataTypeConflictHandler.REPLACE_HANDLER));
	}

	private void farPointer(String name, DataType target) {
		typedef(name, new PointerDataType(target, FAR_POINTER_SIZE, dtm));
	}

	// Members are laid out back to back: every Win16 struct used here is
	// already naturally aligned, so no packing rules are needed.
	private void struct(String name, String... members) {
		StructureDataType struct = new StructureDataType(CATEGORY, name, 0, dtm);
		for (String member : members) {
			Matcher declarator = DECLARATOR.matcher(member);
			if (!declarator.matches()) {
				throw new IllegalArgumentException("Bad member of " + name + ": " + member);
			}
			DataType type = type(declarator.group(1));
			if (declarator.group(3) != null) {
				int count = Integer.parseInt(declarator.group(3));
				type = new ArrayDataType(type, count, type.getLength(), dtm);
			}
			struct.add(type, declarator.group(2), null);
		}
		types.put(name, dtm.addDataType(struct, DataTypeConflictHandler.REPLACE_HANDLER));
	}
}
