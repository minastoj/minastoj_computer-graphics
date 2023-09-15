CSCI 420 HW 1 - Mina Stojanovic

ESC key -- exits program
'x' key -- takes screenshot
't' key -- enters TRANSLATE controlState
SHIFT key -- enters SCALE controlState

If no keys are pressed, you are in the ROTATE controlState.

NOTE: Pressing the OPTION key slightly alters the movement in any of these states:

    When you're in the ROTATE state, and not holding OPTION, the image will pitch and yaw. However, when you hold OPTION while in the ROTATE state, the image will instead roll.
    
    When you're in the TRANSLATE state and not holding OPTION, the image will move left & right, and forward & back. However, if you hold OPTION while in the TRANSLATE state, the image will move up and down instead.

    When you're in the SCALE state and not holding OPTION, the image will stretch up & down, and left & right. However, if you hold OPTION while in the SCALE state, the image will additionally stretch forward and back.

'1' key -- point mode
'2' key -- line mode
'3' key -- triangle mode
'4' key -- smoothened triangle mode

The code can successfully generate all of the aforementioned modes and switch between them easily. It can also properly scale, rotate, and translate.