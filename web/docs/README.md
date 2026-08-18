Features:

-Positioning
-Script Attachment
-Input Mechanics (Key Presses, mouse movement and clicks)


Sunday, 3 November 2024

Implemented Box2D.
Added World and Body-.Rigidbody classes.
Audio support coming soon...

Tuesday, 5 November 2024

Audio support!
We can now play, stop and loop audio files.
However, we are using SDL_mixer, which is kind of a pain...

Sprites contain a vector of shared pointers to 'Parts' with their own background assets, allowing for Hierarchical, modular components.

Wednesday, 6 November 2024

I have, on purpose, taken out DirectWrite... replacing it with SDL_ttf.
Integrated GamePad support, using SDL_GamePad.

11 November

Refactored Position to Point2D to fit in line with Vector2D

