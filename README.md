# MHQ3A

This is a personal vanity project rather than anything intended as a serious engine replacement. 

I wanted to see if it was possible to implement Q3A's "shaders" as actual real shaders, and in particular I was interested in moving per-vertex stuff - sky, turb, environment mapping - to per-pixel.

I chose D3D 9 for reasons of simplicity, because it has a reasonably powerful and reasonably civilized shading language and DrawPrimitiveUP.

It should be robust enough so long as you don't try anything fancy - multiple monitors, alt-tabbing, forcing settings through your GPU's control panel.

You'll be able to play Q3A but don't go looking for bug fixes or modern features here. That's not it's purpose. 
