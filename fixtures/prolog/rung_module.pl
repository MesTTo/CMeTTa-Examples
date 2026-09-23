% The same two operations behind a Prolog MODULE header, so the use_module
% doors have something to load that a plain consult would not treat the same
% way: a module exports a list and hides everything else.
:- module(rung_module, [rung_module_triple/2]).

rung_module_triple(X, Y) :- Y is X * 3.
rung_module_hidden(secret).
