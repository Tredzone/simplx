# Tutorial #11 - Keyboard Actor

This tutorial shows how to asynchronously poll the keyboard via the native KeyboardActor.

The workflow terminates when 'q' is pressed, by notifying a condition_variable (via a wrapper).
