% Loaded through lib_zar's predicate-style import_prolog_functions_from_module_pred.
:- module(rung_pred_module, [rung_pred_five/2]).

rung_pred_five(X, Y) :- Y is X * 5.
