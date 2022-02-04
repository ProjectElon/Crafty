#pragma once

// note(harlequin): the key and button codes are taken from glfw3.h

/* The unknown key */
#define MC_KEY_UNKNOWN            -1

/* Printable keys */
#define MC_KEY_SPACE              32
#define MC_KEY_APOSTROPHE         39  /* ' */
#define MC_KEY_COMMA              44  /* , */
#define MC_KEY_MINUS              45  /* - */
#define MC_KEY_PERIOD             46  /* . */
#define MC_KEY_SLASH              47  /* / */
#define MC_KEY_0                  48
#define MC_KEY_1                  49
#define MC_KEY_2                  50
#define MC_KEY_3                  51
#define MC_KEY_4                  52
#define MC_KEY_5                  53
#define MC_KEY_6                  54
#define MC_KEY_7                  55
#define MC_KEY_8                  56
#define MC_KEY_9                  57
#define MC_KEY_SEMICOLON          59  /* ; */
#define MC_KEY_EQUAL              61  /* = */
#define MC_KEY_A                  65
#define MC_KEY_B                  66
#define MC_KEY_C                  67
#define MC_KEY_D                  68
#define MC_KEY_E                  69
#define MC_KEY_F                  70
#define MC_KEY_G                  71
#define MC_KEY_H                  72
#define MC_KEY_I                  73
#define MC_KEY_J                  74
#define MC_KEY_K                  75
#define MC_KEY_L                  76
#define MC_KEY_M                  77
#define MC_KEY_N                  78
#define MC_KEY_O                  79
#define MC_KEY_P                  80
#define MC_KEY_Q                  81
#define MC_KEY_R                  82
#define MC_KEY_S                  83
#define MC_KEY_T                  84
#define MC_KEY_U                  85
#define MC_KEY_V                  86
#define MC_KEY_W                  87
#define MC_KEY_X                  88
#define MC_KEY_Y                  89
#define MC_KEY_Z                  90
#define MC_KEY_LEFT_BRACKET       91  /* [ */
#define MC_KEY_BACKSLASH          92  /* \ */
#define MC_KEY_RIGHT_BRACKET      93  /* ] */
#define MC_KEY_GRAVE_ACCENT       96  /* ` */
#define MC_KEY_WORLD_1            161 /* non-US #1 */
#define MC_KEY_WORLD_2            162 /* non-US #2 */

/* Function keys */
#define MC_KEY_ESCAPE             256
#define MC_KEY_ENTER              257
#define MC_KEY_TAB                258
#define MC_KEY_BACKSPACE          259
#define MC_KEY_INSERT             260
#define MC_KEY_DELETE             261
#define MC_KEY_RIGHT              262
#define MC_KEY_LEFT               263
#define MC_KEY_DOWN               264
#define MC_KEY_UP                 265
#define MC_KEY_PAGE_UP            266
#define MC_KEY_PAGE_DOWN          267
#define MC_KEY_HOME               268
#define MC_KEY_END                269
#define MC_KEY_CAPS_LOCK          280
#define MC_KEY_SCROLL_LOCK        281
#define MC_KEY_NUM_LOCK           282
#define MC_KEY_PRINT_SCREEN       283
#define MC_KEY_PAUSE              284
#define MC_KEY_F1                 290
#define MC_KEY_F2                 291
#define MC_KEY_F3                 292
#define MC_KEY_F4                 293
#define MC_KEY_F5                 294
#define MC_KEY_F6                 295
#define MC_KEY_F7                 296
#define MC_KEY_F8                 297
#define MC_KEY_F9                 298
#define MC_KEY_F10                299
#define MC_KEY_F11                300
#define MC_KEY_F12                301
#define MC_KEY_F13                302
#define MC_KEY_F14                303
#define MC_KEY_F15                304
#define MC_KEY_F16                305
#define MC_KEY_F17                306
#define MC_KEY_F18                307
#define MC_KEY_F19                308
#define MC_KEY_F20                309
#define MC_KEY_F21                310
#define MC_KEY_F22                311
#define MC_KEY_F23                312
#define MC_KEY_F24                313
#define MC_KEY_F25                314
#define MC_KEY_KP_0               320
#define MC_KEY_KP_1               321
#define MC_KEY_KP_2               322
#define MC_KEY_KP_3               323
#define MC_KEY_KP_4               324
#define MC_KEY_KP_5               325
#define MC_KEY_KP_6               326
#define MC_KEY_KP_7               327
#define MC_KEY_KP_8               328
#define MC_KEY_KP_9               329
#define MC_KEY_KP_DECIMAL         330
#define MC_KEY_KP_DIVIDE          331
#define MC_KEY_KP_MULTIPLY        332
#define MC_KEY_KP_SUBTRACT        333
#define MC_KEY_KP_ADD             334
#define MC_KEY_KP_ENTER           335
#define MC_KEY_KP_EQUAL           336
#define MC_KEY_LEFT_SHIFT         340
#define MC_KEY_LEFT_CONTROL       341
#define MC_KEY_LEFT_ALT           342
#define MC_KEY_LEFT_SUPER         343
#define MC_KEY_RIGHT_SHIFT        344
#define MC_KEY_RIGHT_CONTROL      345
#define MC_KEY_RIGHT_ALT          346
#define MC_KEY_RIGHT_SUPER        347
#define MC_KEY_MENU               348
#define MC_KEY_LAST               MC_KEY_MENU

#define MC_MOUSE_BUTTON_1         0
#define MC_MOUSE_BUTTON_2         1
#define MC_MOUSE_BUTTON_3         2
#define MC_MOUSE_BUTTON_4         3
#define MC_MOUSE_BUTTON_5         4
#define MC_MOUSE_BUTTON_6         5
#define MC_MOUSE_BUTTON_7         6
#define MC_MOUSE_BUTTON_8         7
#define MC_MOUSE_BUTTON_LAST      MC_MOUSE_BUTTON_8
#define MC_MOUSE_BUTTON_LEFT      MC_MOUSE_BUTTON_1
#define MC_MOUSE_BUTTON_RIGHT     MC_MOUSE_BUTTON_2
#define MC_MOUSE_BUTTON_MIDDLE    MC_MOUSE_BUTTON_3