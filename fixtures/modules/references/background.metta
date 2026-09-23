; Purpose: supply a separate rows-only home for background reference loading.
; [tested: examples/ch20-extending-the-engine/20-04-modules-and-the-catalog/13-reference_loading.metta; commit=90ba93eb8f6e98ebfefc55416859bf13de6a8427].
(: background-value (-> Number Number))
(= (background-value $x) (+ $x 1))
