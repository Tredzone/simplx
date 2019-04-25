
# Tutorial #2 - Hello World

In this tutorial, you will learn to define your own actor by inheriting the *Actor* class

```c++
class HelloWorldActor : public Actor, public Actor::Callback
{
  registerCallback(*this);
}
```

Once registered, the event-loop managing this actor will asynchronously call HelloWorldActor::onCallback(). In the example, this method is defined as follows,

```c++
void onCallback(void)
{
  cout << "Hello World" << endl;
}
```
