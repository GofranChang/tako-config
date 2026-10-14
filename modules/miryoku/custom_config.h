// Copyright 2021 Manna Harbour
// https://github.com/manna-harbour/miryoku

#define MIRYOKU_ALPHAS_QWERTY

#define MIRYOKU_NAV_VI

// Tako overrides: output toggle works even with Shift held; Sym uses physical I/comma/dot.
#define MIRYOKU_LAYER_MEDIA \
U_BOOT,                &u_to_U_TAP,           &u_to_U_EXTRA,         &u_to_U_BASE,          U_NA,                  U_RGB_EFF,             U_RGB_HUI,             U_RGB_SAI,             U_RGB_BRI,             U_RGB_TOG, \
&kp LGUI,              &kp LALT,              &kp LCTRL,             &kp LSHFT,             U_NA,                  &kp C_PREV,            &kp C_VOL_DN,          &kp C_VOL_UP,          &kp C_NEXT,            U_EP_TOG, \
U_NA,                  &kp RALT,              &u_to_U_FUN,           &u_to_U_MEDIA,         U_NA,                  &u_bt_sel_0,           &u_bt_sel_1,           &u_bt_sel_2,           &u_bt_sel_3,           &out OUT_TOG, \
U_NP,                  U_NP,                  U_NA,                  U_NA,                  U_NA,                  &kp C_STOP,            &kp C_PP,              &kp C_MUTE,            U_NP,                  U_NP

#define MIRYOKU_LAYER_SYM \
&kp LBRC,              &kp AMPS,              &kp ASTRK,             &kp LPAR,              &kp RBRC,              U_NA,                  &u_to_U_BASE,          &kp ESC,               &u_to_U_TAP,           U_BOOT, \
&kp COLON,             &kp DLLR,              &kp PRCNT,             &kp CARET,             &kp PLUS,              U_NA,                  &kp LSHFT,             &kp LCTRL,             &kp LALT,              &kp LGUI, \
&kp TILDE,             &kp EXCL,              &kp AT,                &kp HASH,              &kp PIPE,              U_NA,                  &u_to_U_SYM,           &kp LPAR,              &kp RPAR,              U_NA, \
U_NP,                  U_NP,                  &kp LPAR,              &kp RPAR,              &kp UNDER,             U_NA,                  U_NA,                  U_NA,                  U_NP,                  U_NP
