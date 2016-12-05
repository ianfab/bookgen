# bookgen
EPD opening book generator based on [Stockfish with variant support](https://github.com/ddugovic/Stockfish).

## Usage

First, set UCI options and set up the position the book generation should start from. Especially relevant options are:
- `UCI_Variant` to set the chess variant.
- `CentipawnRange` to define the range of good moves in centipawns.
- `DepthFactor` to modify `CentipawnRange` in dependence of the depth from the root position. The effective range is calculated as `CentipawnRange * (DepthFactor / 100) ^ depth`.
- `multipv` to define the maximum branching factor of opening lines during book generation.

Then an opening book can be generated using `generate n` to obtain an opening book with positions after n ply.
Optional arguments are movetime, depth, and nodestime to set limits for the search on each position as they are used for the `go` command.

### Example
The following input generates a small crazyhouse opening book of depth 3 ply, where a search of one second with multipv=5 is performed on each position. Only moves within a range of `50 * 0.9 ^ depth` centipawns compared to the best move are considered during the generation of the book tree.
```
uci
setoption name CentipawnRange value 50
setoption name DepthFactor value 90
setoption name multipv value 5
setoption name UCI_Variant value crazyhouse
ucinewgame
position startpos
generate 3 movetime 1000
```
