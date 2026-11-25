# Unreal: bugs found in play, 2026-11-25

The owner played the Unreal build and reported light that jumped, a low frame rate, music that dropped, a camera
that stuck, and cars that looked broken and shook when driven. What was found and mended is in `Game/README.md`
(The eye and the night, Sound, Furnished homes, Vehicles, The camera).

| Picture | What it shows |
|---|---|
| `cars_before_after.jpg` | Four vehicles taken with `-BrixDrive=`, before and after. Before: each car a dark hollow, its riders' legs showing through the doors: the models' triangles went round the wrong way for Unreal, so each part's inside was drawn and its outside left out. After: the Soft box cars of `Art/Reference/vehicles_soft_box.png`: one accent colour, a cream roof, matte |
| `bus_camera_before_after.jpg` | Riding a bus: before, the camera 11 m behind a seat at the front of an 11 m bus, over its roof; after, 15 m back and nearly level, behind the bus (in the first, under a tree, the bottom of its crown is in the picture: crowns do not stop the camera) |
| `mall_people_before_after.jpg` | Eastgate at 02:00, 22:30 and noon. Before: the player black in the court at night (the hall glows by itself, and people were lit only by the sky's light coming in). After: a light for people alone with the camera in the hall |
