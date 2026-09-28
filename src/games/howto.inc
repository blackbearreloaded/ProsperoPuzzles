// ProsperoPuzzles - How-to-play rules, one short paragraph per game.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Written for this app (summaries, not upstream manual text). Lines are
// separated by '\n'; each becomes a paragraph on the How to play card.

// HOWTO(id, rules, controls)
HOWTO(blackbox,
      "Balls are hidden inside the box. Fire a laser from any edge: it may pass straight through, "
      "be absorbed by a ball (H), be reflected back (R), or come out elsewhere after being "
      "deflected.\nMark where you think every ball is, then press Submit when you are sure.",
      "Cross on the edge fires a laser. Cross inside the box places a ball guess; Square marks a "
      "square as certainly empty.")
HOWTO(bridges,
      "Each circle is an island showing how many bridges touch it. Bridges run straight "
      "horizontally or vertically, never cross each other, and at most two join the same pair "
      "of islands.\nEvery island must be connected into one network.",
      "With the stick pointer, hold Cross on an island and drag to a neighbour to build a bridge; "
      "repeat to double it, again to remove it. Square marks an island as finished.")
HOWTO(cube,
      "Roll the cube around the grid. Rolling onto a blue square picks it up on the cube's "
      "bottom face; rolling a blue face onto a plain square puts it back down.\nFinish with all "
      "the blue on the cube, in as few rolls as you can.",
      "D-pad or stick rolls the cube.")
HOWTO(dominosa,
      "The grid is a full set of dominoes with the lines between them erased. Every pair of "
      "numbers from 0-0 up to the highest double appears exactly once.\nDraw the domino "
      "boundaries back in.",
      "Cross between two squares places a domino there. Square draws an edge you know is a "
      "boundary.")
HOWTO(fifteen,
      "Slide the numbered tiles into the gap until they read in order, left to right and top to "
      "bottom, with the gap in the bottom-right corner.",
      "Move to a tile in line with the gap and press Cross to slide it.")
HOWTO(filling,
      "Fill every square with a number. Squares with the same number that touch form a region, "
      "and each region's size must equal its number: a 3 belongs to exactly three connected 3s."
      "\nTwo regions of the same size cannot touch along an edge.",
      "Select a square with Cross and press Triangle to pick a number.")
HOWTO(flip,
      "Pressing a square flips it and a fixed pattern of its neighbours between light and dark. "
      "The pattern is drawn on each square.\nLight up the whole grid.",
      "Cross flips the selected square.")
HOWTO(flood,
      "Starting from the top-left corner, flood-fill the connected area with a new colour; it "
      "absorbs every touching square of that colour.\nTurn the whole grid one colour within the "
      "move limit.",
      "Select a square of the colour you want and press Cross.")
HOWTO(galaxies,
      "Split the grid into regions, one around each dot. Every region must be symmetric under a "
      "half turn about its dot.",
      "Cross on a grid line draws or erases an edge. Square next to a dot draws an arrow linking "
      "a square to that dot.")
HOWTO(g2048,
      "Slide every tile on the board at once. When two tiles with the same number collide they "
      "merge into their sum, and a new tile appears after each move.\nReach 2048, then keep going "
      "for a higher score. The game ends when no move is left.",
      "D-pad or stick slides the tiles. L1 undoes the last move.")
HOWTO(guess,
      "A secret row of coloured pegs is hidden. Guess a row; each black marker says a peg is the "
      "right colour in the right place, each white one a right colour in the wrong place."
      "\nFind the code within the allowed number of guesses.",
      "Select a peg slot and press Triangle to choose its colour, then Cross on the check button "
      "to submit the row.")
HOWTO(inertia,
      "Your ball slides until it hits a wall, lands on a stop square, or collects a gem. Mines "
      "end the game.\nCollect every gem without hitting a mine.",
      "D-pad or stick picks one of eight directions (use diagonals with the stick) and Cross "
      "moves.")
HOWTO(keen,
      "Fill the grid so each row and column holds every number exactly once. Each outlined cage "
      "shows a target and an operation: the numbers in the cage must produce that target.",
      "Select a square and press Triangle for the number pad. Square toggles pencil marks.")
HOWTO(lightup,
      "Place light bulbs in white squares. A bulb lights its row and column until blocked by a "
      "black square.\nLight every white square, never let two bulbs shine on each other, and give "
      "each numbered black square exactly that many neighbouring bulbs.",
      "Cross places or removes a bulb. Square marks a square that cannot hold one.")
HOWTO(loopy,
      "Draw one closed loop along the grid lines, with no branches or crossings. A number in a "
      "cell says how many of that cell's edges the loop uses.",
      "Cross on an edge draws it; Square marks it as definitely not part of the loop.")
HOWTO(magnets,
      "Each domino shape is either a magnet (a plus and a minus end) or blank. The numbers "
      "around the edge count pluses and minuses in each row and column.\nLike poles may not touch "
      "along an edge.",
      "Cross cycles a domino through magnet orientations; Square marks it as blank.")
HOWTO(map,
      "Colour every region with one of four colours so no two neighbouring regions share one. "
      "Some regions start coloured.",
      "Select a region and press Triangle to pick its colour. With the stick pointer you can "
      "also drag a colour from one region to another.")
HOWTO(mines,
      "Some squares hide mines. Opening a safe square shows how many of its eight neighbours are "
      "mines.\nOpen every safe square. The first square you open is always safe.",
      "Cross opens a square; Square plants a flag. Cross on an opened number whose flags are "
      "complete opens its remaining neighbours.")
HOWTO(mosaic,
      "Shade squares in the grid. Each clue counts the shaded squares in the 3x3 block around "
      "it, including its own square.",
      "Cross shades a square; Square marks it as blank.")
HOWTO(net,
      "The tiles form a network of wires, scrambled by rotation. Rotate tiles until every piece "
      "is connected to the centre with no loose ends and no loops.",
      "Cross rotates a tile; Square rotates it the other way.")
HOWTO(netslide,
      "Like Net, but instead of rotating tiles you slide whole rows and columns, wrapping at the "
      "edges.\nConnect every piece to the centre with no loose ends.",
      "Move to an arrow at the edge of the board and press Cross to slide that row or column.")
HOWTO(palisade,
      "Split the grid into regions all of the same size. A number in a square says how many of "
      "its four edges are region boundaries.",
      "Cross on an edge draws it; Square marks it as not a boundary.")
HOWTO(pattern,
      "Shade squares to reveal a picture. The numbers by each row and column list the lengths of "
      "its shaded runs, in order, with at least one gap between runs.",
      "Cross shades, Square marks empty. With the stick pointer, hold and drag to fill a line.")
HOWTO(pearl,
      "Draw a single loop through square centres. The loop turns on every black pearl and goes "
      "straight through both squares next to it; it goes straight through every white pearl and "
      "turns in at least one square next to it.",
      "With the stick pointer, hold Cross and drag to draw the loop; Square marks a link as impossible.")
HOWTO(pegs,
      "Jump a peg over a neighbouring peg into an empty hole; the jumped peg is removed.\nFinish "
      "with a single peg on the board.",
      "With the stick pointer, hold Cross on a peg and drag it to the empty hole.")
HOWTO(range,
      "Shade some squares black. Numbered squares stay white; each number counts the white "
      "squares visible from it in straight lines, including itself.\nBlack squares may not touch "
      "along an edge, and all white squares must connect.",
      "Cross shades a square; Square marks it as white.")
HOWTO(rect,
      "Divide the grid into rectangles so each contains exactly one number, equal to its area.",
      "With the stick pointer, hold Cross and drag out a rectangle.")
HOWTO(samegame,
      "Remove groups of two or more touching squares of the same colour. Squares above fall "
      "down, and empty columns close up.\nClear the board; bigger groups score more.",
      "Cross selects a group; press Cross again to remove it.")
HOWTO(signpost,
      "Link every square into one path, numbered 1 to the last, where each square's arrow points "
      "toward the next square in the path (any distance away).",
      "With the stick pointer, hold Cross on a square and drag to the square it leads to.")
HOWTO(singles,
      "Shade squares so no number repeats in any row or column. Shaded squares may not touch "
      "along an edge, and all unshaded squares must stay connected.",
      "Cross shades a square; Square circles one you know stays.")
HOWTO(sixteen,
      "Slide whole rows and columns, wrapping around, until the tiles read in order.",
      "Move to an arrow at the edge and press Cross to slide that row or column.")
HOWTO(slant,
      "Put a diagonal line in every square. A number on a grid point says how many lines meet "
      "there.\nThe lines must never form a closed loop.",
      "Cross draws one diagonal; Square draws the other.")
HOWTO(solo,
      "Fill the grid so each row, column and outlined block contains every digit exactly once.",
      "Select a square and press Triangle for the number pad. Square toggles pencil marks.")
HOWTO(tenfold,
      "Select a group of touching equal numbers to merge it into one tile worth one more; the "
      "tiles above fall into the gaps and new ones drop in.\nClimb all the way to ten, then "
      "keep playing for score.",
      "Cross selects then merges. Square shows a hint. L1 undoes the last merge.")
HOWTO(tents,
      "Place one tent next to each tree (up, down, left or right) so every tree has its own tent. "
      "Tents may not touch each other, even diagonally.\nThe numbers give the tent count for each "
      "row and column.",
      "Cross places a tent; Square marks grass.")
HOWTO(towers,
      "Fill the grid with towers of heights 1 to N, each height once per row and column. A clue "
      "outside the grid counts the towers visible from there; taller towers hide shorter ones.",
      "Select a square and press Triangle for the number pad. Square toggles pencil marks.")
HOWTO(tracks,
      "Lay a railway from the A entrance to the B exit through the grid. It never branches or "
      "crosses itself. The numbers count the track squares in each row and column.",
      "With the stick pointer, hold Cross and drag to lay track; Square marks a square with no track.")
HOWTO(twiddle,
      "Rotate square blocks of tiles until the grid reads in order.",
      "Cross rotates the block under the cursor clockwise; Square rotates it anticlockwise.")
HOWTO(undead,
      "Fill every empty square with a ghost, vampire or zombie. The numbers around the edge "
      "count the monsters seen along each line of sight. Mirrors bounce the view; ghosts are "
      "only seen in mirrors, vampires only directly, and zombies both ways.",
      "Select a square and press Triangle to choose a monster.")
HOWTO(unequal,
      "Fill the grid so each row and column contains every number once. The inequality signs "
      "between squares must hold.",
      "Select a square and press Triangle for the number pad. Square toggles pencil marks.")
HOWTO(unruly,
      "Colour every square black or white. Each row and column holds equal numbers of each "
      "colour, and no three squares in a line may share a colour.",
      "Cross cycles black, white and empty.")
HOWTO(untangle,
      "The dots are joined by lines that cross. Move the dots until no two lines cross.",
      "With the stick pointer, hold Cross on a dot, drag it, and release to drop it.")
