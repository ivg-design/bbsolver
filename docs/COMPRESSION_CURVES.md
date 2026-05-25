# bbsolver compression curves

Reduction-vs-tolerance table at tolerances [0.01, 0.1, 0.5, 1.0, 5.0],
collected by running `bbsolver solve` over each fixture and capturing the
resulting `key_count` and `max_err`. The fixtures listed below are
representative bake samples; package-local minimal versions live under
`examples/json/`.

Each cell shows K (key count) at the given tolerance. Tighter tolerance → more keys.
Use this table to pick a tolerance per property type that meets quality without over-keying.

| fixture                    | kind           | N   | K@0.01 | K@0.1 | K@0.5 | K@1 | K@5 | err@0.01 | err@0.1 | err@0.5 | err@1   | err@5   |
| ---                        | ---            | --- | ---    | ---   | ---   | --- | --- | ---      | ---     | ---     | ---     | ---     |
| bouncing_ball_2d.bbsm.json | TwoD_Spatial   | 97  | 6      | 6     | 6     | 5   | 4   | 4.5e-07  | 4.5e-07 | 4.5e-07 | 0.5     | 4.9     |
| color_pulse.bbsm.json      | Color          | 73  | 3      | 3     | 3     | 3   | 3   | 0.005    | 0.063   | 0.063   | 0.45    | 0.45    |
| sin_blend_smooth.bbsm.json | Scalar         | 145 | 8      | 8     | 6     | 4   | 4   | 0.0078   | 0.0078  | 0.22    | 0.9     | 0.9     |
| slow_arc_3d.bbsm.json      | ThreeD_Spatial | 193 | 13     | 8     | 6     | 6   | 5   | 0.0085   | 0.075   | 0.26    | 0.26    | 2.1     |
| step_function.bbsm.json    | Scalar         | 145 | 8      | 8     | 8     | 8   | 8   | 6.0e-13  | 6.0e-13 | 6.0e-13 | 6.0e-13 | 6.0e-13 |
| wiggle_1d.bbsm.json        | Scalar         | 121 | 31     | 31    | 31    | 31  | 26  | 6.3e-11  | 6.3e-11 | 6.3e-11 | 6.3e-11 | 2.7     |
