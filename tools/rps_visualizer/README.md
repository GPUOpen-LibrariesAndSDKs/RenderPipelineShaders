# RPS Visualizer Tool

## Usage

The RPS Visualizer tool can be integrated as a library into applications using RPS and ImGui to visualize various data of the RPS render graphs. [/tools/rpsl_explorer](../rpsl_explorer/) makes use of this library and can serve as an example of integration.

## Views 

There are currently 3 different views available which can be activated from collapsible headers on the left. 

- The "Resources" view shows lifetime and access states of all resources known to the rps render graph. These are e.g. user defined commands, built-ins like resource clears, etc. Transitions are displayed as purple diamonds. Hovering your mouse over the widgets brings up a tool tip showing details about the resources, accesses and transitions.
- The "Heaps" view shows lifetimes of resources over the same timeline but combined with the memory placements in their heaps. This is intended to intuitively visualize how resources are aliased over the time of the frame.
- The "Graph" view shows the DAG that RPS builds internally for scheduling. This visualizes the interconnections and dependencies between graph nodes, where each dependency is specified by an edge in the graph.

## Controls

| Action                                       | Description                                                                                                      |
| -------------------------------------------- | ---------------------------------------------------------------------------------------------------------------- |
| Mouse Wheel                                  | Zoom vertically in the parent view.                                                                              |
| CTRL + Mouse Wheel                           | Zoom horizontally on the current grid w.r.t. the current mouse position on it.                                   |
| SHIFT + Mouse Wheel                          | Zoom vertically on the current grid w.r.t. the current mouse position on it (only for the heap and graph views). |
| Mouse Click + Mouse Movement + Mouse Release | Select a horizontal and vertical (only for the heap view) range for visual highlighting.                         |
| CTRL + Z                                     | Zoom horizontally to the selected range.                                                                         |
| CTRL + B                                     | Zoom vertically to the selected range (only for the heap view).                                                  |
| W/A/S/D                                      | Move up/left/down/right by just a tiny amount (Hold for continuous movement).                                    |
| Mouse Click at graph node or transition      | Highlight all incoming and outgoing connections (graph view only).                                               |
