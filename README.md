# rat-elimination

C implementation of [Rebola-Pardo and Weissenbacher "RAT Elimination"](https://publik.tuwien.ac.at/files/publik_293387.pdf)


## TODO
### Working with the correct RAT pivot
Currently the LRAT clause literals are accepted as sorted and keep sorted by merging them during resolutions.
But the information on the RAT pivot is lost by using this ordering.
Solutions: update preprocessing and store pivot somewhere in clause.
### Random clause apearance.
after running the paper example on bdd-solver and trimming + transforming getting the following for 158 and 159, which in the context of the proof is false!

But those two clauses can be made up, since no constraints exist on the literals, if the chain is changed to `0 0` the proof can be verified.

```
90 3 39 -40 0 86 85 11 19 0
104 3 43 -44 0 99 100 13 20 0
...
158 39 -40 -56 0 230 90 0
159 43 -44 -56 0 230 104 0
...
230 -51 67 -68 0 120 129 226 134 0
```

Also proof verifiers have a hard time to verify non trimmed proof since.
