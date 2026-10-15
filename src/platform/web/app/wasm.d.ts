// The module built by the CMake `wasm` preset from src/platform/web/bridge/bridge.c.
// Pointers are offsets into HEAPU8; C bools come back as 0 or 1.
declare module '@wasm/jthief.js' {
  type Pointer = number;

  export interface JewelThiefModule {
    HEAPU8: Uint8Array;
    _malloc(size: number): Pointer;
    _free(pointer: Pointer): void;

    _jt_open(data: Pointer, size: number): number;
    _jt_missing_name(): Pointer;
    _jt_width(): number;
    _jt_height(): number;

    _jt_start(seed: number, speed: number, cheat: boolean): void;
    _jt_new_game(speed: number, cheat: boolean): void;
    _jt_tick(): number;
    _jt_mouse_move(x: number, y: number): number;
    _jt_set_focus(focused: boolean): void;
    _jt_level(): number;
    _jt_final_score(): number;
    _jt_cursor_faces_left(x: number, y: number): number;
    _jt_toggle_god_mode(): number;
    _jt_debug_change_level(delta: number): void;

    _jt_render(): Pointer;
    _jt_status_text(index: number): Pointer;
    _jt_status_x(index: number): number;
    _jt_status_y(index: number): number;

    _jt_cursor_rgba(index: number): Pointer;
    _jt_cursor_width(index: number): number;
    _jt_cursor_height(index: number): number;
    _jt_cursor_hotspot_x(index: number): number;
    _jt_cursor_hotspot_y(index: number): number;

    _jt_text(id: number): Pointer;
    _jt_dialog_text(name: Pointer): Pointer;
    _jt_ending_message(): Pointer;

    _jt_parse_speed(value: Pointer): number;
    _jt_parse_level_wait(value: Pointer): number;
    _jt_parse_cheat(value: Pointer): number;
    _jt_speed_value(speed: number): Pointer;
    _jt_flag_value(flag: boolean): Pointer;

    _jt_scores_load(scores: Pointer, names: Pointer): void;
    _jt_scores_rank(score: number): number;
    _jt_scores_insert(rank: number, name: Pointer, score: number): void;
    _jt_scores_name(index: number): Pointer;
    _jt_scores_score(index: number): number;
    _jt_scores_value(): Pointer;
    _jt_names_value(): Pointer;
  }

  export default function createJewelThief(): Promise<JewelThiefModule>;
}
