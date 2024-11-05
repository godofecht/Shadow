Features:

-Positioning
-Script Attachment
-Input Mechanics (Key Presses, mouse movement and clicks)


Sunday, 3 November 2024

Implemented Box2D.
Added World and Body-.Rigidbody classes.
Audio support coming soon...

Sunday, 5 November 2024

Audio support!
We can now play, stop and loop audio files.
However, we are using SDL_mixer, which is kind of a pain...

Sprites contain a vector of shared pointers to 'Parts' with their own background assets, allowing for Hierarchical, modular components.