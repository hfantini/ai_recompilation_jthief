import java.util.ArrayList;
import java.util.List;
import java.util.Optional;

import ghidra.program.model.address.Address;
import ghidra.program.model.address.SegmentedAddress;
import ghidra.program.model.address.SegmentedAddressSpace;
import ghidra.program.model.listing.Program;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryAccessException;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.util.task.TaskMonitor;

/**
 * Finds the switch jump tables Microsoft C emits in 16-bit code: a
 * {@code jmp cs:[bx+table]} followed by an inline table of near offsets.
 * The table stores no length, but the compiler places it right before the
 * first case body, so it ends where the lowest target begins.
 */
public final class Win16SwitchTables {

	public record SwitchTable(Address jump, Address table, List<Address> targets) {
	}

	// jmp word ptr cs:[bx+disp16]
	private static final byte[] JMP_CS_BX_DISP16 = { 0x2E, (byte) 0xFF, (byte) 0xA7 };
	private static final int MAX_ENTRIES = 256;

	private Win16SwitchTables() {
	}

	public static List<SwitchTable> find(Program program, TaskMonitor monitor)
			throws MemoryAccessException {
		Memory memory = program.getMemory();
		List<SwitchTable> tables = new ArrayList<>();
		for (MemoryBlock block : memory.getBlocks()) {
			if (!block.isInitialized()) {
				continue;
			}
			Address cursor = block.getStart();
			while (cursor != null && !monitor.isCancelled()) {
				Address jump = memory.findBytes(cursor, block.getEnd(), JMP_CS_BX_DISP16, null,
					true, monitor);
				if (jump == null) {
					break;
				}
				readTable(memory, block, (SegmentedAddress) jump).ifPresent(tables::add);
				cursor = jump.next();
			}
		}
		return tables;
	}

	private static Optional<SwitchTable> readTable(Memory memory, MemoryBlock block,
			SegmentedAddress jump) throws MemoryAccessException {
		SegmentedAddressSpace space = (SegmentedAddressSpace) jump.getAddressSpace();
		int segment = jump.getSegment();
		int tableOffset =
			Short.toUnsignedInt(memory.getShort(jump.add(JMP_CS_BX_DISP16.length)));

		List<Address> targets = new ArrayList<>();
		int lowestTarget = Integer.MAX_VALUE;
		for (int offset = tableOffset; offset < lowestTarget; offset += 2) {
			Address entry = space.getAddress(segment, offset);
			if (targets.size() == MAX_ENTRIES || !block.contains(entry)) {
				return Optional.empty();
			}
			int targetOffset = Short.toUnsignedInt(memory.getShort(entry));
			Address target = space.getAddress(segment, targetOffset);
			// A real table only jumps forward, past itself, within its own segment.
			if (targetOffset <= offset || !block.contains(target)) {
				return Optional.empty();
			}
			lowestTarget = Math.min(lowestTarget, targetOffset);
			targets.add(target);
		}
		return Optional.of(
			new SwitchTable(jump, space.getAddress(segment, tableOffset), targets));
	}
}
