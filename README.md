# Brick-shooting-game
To Open Game :<br/>
  type following commands
  ```
    $ make
    $ ./sample2D
  ```

RULES :<br/>
  Collecting black brick ends game
  collecting is not possible when two baskets overlap
  collecting  red or green  bricks in respective baskets gives +20 points
  shooting  red or green bricks will give you -5 points
  shooting black brick will give you +50 points
  shooting 5 red or 5 green bricks will end the game
  cannon needs 1 second to activate to after firing a laser
  lasers can be reflected by mirrors
  lasers are absorbed by walls
  SCORE IS DISPLAYED IN TERMINAL

CONTROLS :

  KEYBOARD :

      a     -   tilt cannon up
      d     -   tilt cannon down
      s     -   move cannon down
      f     -   move cannon up
    space   -   fire laser
      m     -   increase brick falling speed
      n     -   decrease brick falling speed

    up,down arrows          - zoom in and zoom out
    left,right arrows       - pan the scene
    left alt + left arrow   - move red basket left side
    left alt + right arrow  - move red basket right side
    left ctrl + left arrow  - move green basket left side
    left ctrl + right arrow - move green basket right side

  MOUSE :

    left click and hold       - select basket or cannon and drag
        left click            - to fire laser where it is clicked
        scroll                - zoom in and zoom out
    right click and hold - pan left and right
