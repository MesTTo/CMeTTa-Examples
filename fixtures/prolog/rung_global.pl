% Loaded through lib_import's import_prolog_functions_from_module, which is
% use_module_global followed by an all-or-nothing registration.
:- module(rung_global, [rung_global_six/2]).

rung_global_six(X, Y) :- Y is X * 6.
