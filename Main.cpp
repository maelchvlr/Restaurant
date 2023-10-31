#include <thread>
#include <condition_variable>
#include <mutex>
#include <iostream>
#include <vector>
#include <queue>
#include <random>
#include <string>

// This code is not optimized at all, to optimize it, I should have used only one or two conditions
// To do so, I should have made an enum stating if it was an order, a prepared order, a meal, etc..
// And then do a struct, order, in which I put a variable of this enum.
// The queues could then have been simplified in a single one, and everytime a thread is waken up, it checks in the queue if the object is for itself or not
// And treat it or go back to sleep accordingly.
// It would also have been wise to implement a multiple customer management.
// Overall, I did not go too far in the exercise because i wanted to spend more time on personal projects.


using namespace std;

// We create two structs, one for the Orders, one for the Meals. (useful if we implement multiple customers)
struct Order {
    vector<int> ingredients;
};

struct Meal {
    string meal;
};

// We create an ingredient list to chose from, and two queues, to deposit ad retreive meals and orders.
vector<int> ingredientsList = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
queue<Order> orders;
queue<Meal> meals;

// We then create the corresponding mutex.
mutex Morders;
mutex Mmeals;

// And since, explained above, we're not going to optimize the code, we create conditions for each step of the order handling.
condition_variable orderAvailable;
condition_variable orderTaken;
condition_variable ingredientsReady;
condition_variable mealPrepared;
condition_variable mealTaken;


// We first create our customer function.
void customer() {
    // These two lines are for generating different random numbers for each order, as random in c++ is a little bit shady.
    static thread_local std::mt19937 generator(random_device{}());
    std::uniform_int_distribution<int> distribution(0, 9);

    // We instanciate our order Struct here.
    Order order;

    // Here is an important piece of information if you didn't understand locking properly
    // As you can see, the following code is in between brackets, why is it ?
    // This defines what we call the scope, in other word, the ""zone"" in which the lock_guard will take effect
    // it will automatically unlock when it goes out of scope (here, out of the brackets)
    {

        // generating the lock.
        lock_guard<mutex> lock(Morders);

        // feeding the ingredient vector for each orders.
        for (int i = 0; i < 3; i++) {
            int pick = distribution(generator); 
            order.ingredients.push_back(ingredientsList[pick]);
        }

        // Little prints to know where we are in the code.
        cout << "[customer] choosing ingredients..." << endl;
        this_thread::sleep_for(chrono::seconds(2));
        cout << "[customer] done choosing ingredients" << endl;
        cout << "[customer] Ingredients chosen are: ";
        for (auto ingredient : order.ingredients) {
            cout << ingredient << " ";
        }
        cout << endl;
        cout << "[customer] giving order to the waiter." << endl;

        // Pushing complete order to the orders queue.
        orders.push(order);
    }

    this_thread::sleep_for(chrono::seconds(2));

    // Wakes up one thread in the list of threads waiting on this cond, only one in this code.
    orderAvailable.notify_one();


    // Create the second lock, to wait on a condition
    // You should notice that this time, we used a unique_lock, instead of a lock_guard
    // It is because, unique_lock can be locked and unlocked whenever you want, on top of doing it 
    // Automatically when it goes out of scope
    // We also didn't put brackets this time because out of scope is basically the end of out function.
    unique_lock<mutex> lockMeal(Mmeals);
    mealTaken.wait(lockMeal, [] { return !meals.empty(); });

    // Once notified, we grab the meal.
    Meal meal = meals.front();

    // And empty the queue.
    meals.pop();
    cout << "[customer] Meal " << meal.meal << " received, Yum!" << endl;
}

void waiter() {

    // We start by immediately waiting on a cond.
    unique_lock<mutex> lockOrder(Morders);
    orderAvailable.wait(lockOrder, [] { return !orders.empty(); });

    // Proceed when notified.
    cout << "[waiter] order taken, sending it to the cook" << endl;
    this_thread::sleep_for(chrono::seconds(2));
    cout << "[waiter] order sent" << endl;
    orderTaken.notify_one();

    // This time I manually unlock to show that is it also possible to do it that way.
    lockOrder.unlock();


    // Create another Lock to wait for another condition that's supposed to come way later 
    unique_lock<mutex> lockMeal(Mmeals);
    mealPrepared.wait(lockMeal, [] { return !meals.empty(); });

    // Wakes up whenever the chef is done and ""delivers the food""
    cout << "[waiter] meal ready, delivering it to the client" << endl;
    this_thread::sleep_for(chrono::seconds(2));
    cout << "[waiter] meal delivered" << endl;

    // Notifies the customer that the meal can be takenn.
    mealTaken.notify_one();
}

void cook() {

    // Same thing again and again.
    unique_lock<mutex> lock(Morders);
    orderTaken.wait(lock, [] { return !orders.empty(); });

    cout << "[cook] preparing ingredients..." << endl;

    // Grab the informations out of the queue, without removing the object.
    Order order = orders.front();
    for (int ingredient : order.ingredients) {
        cout << "[cook] preparing " << ingredient << "..." << endl;
        this_thread::sleep_for(chrono::seconds(2));
    }

    cout << "[cook] ingredients ready!" << endl;

    // Notifies the chef.
    ingredientsReady.notify_one();
}

void chef() {

    // Surprisingly the same pattern...
    unique_lock<mutex> lock(Morders);
    ingredientsReady.wait(lock, [] { return !orders.empty(); });

    // This time we 1) get the info, 2) remove the object from the orders queue. 
    Order order = orders.front();
    orders.pop();

    cout << "[chef] ingredients received, starting to cook" << endl;
    this_thread::sleep_for(chrono::seconds(3));
    cout << "[chef] Done cooking, sending meal." << endl;

    // We create another scope, since we don't need to create a unique_lock because we are not waiting for any conditions.
    // We build up the meal name by just adding the ""ingredients"" next to each other in a string.
    {
        lock_guard<mutex> lockMeal(Mmeals);
        Meal meal;
        string foodName;
        for (auto ingredient : order.ingredients)
        {
            foodName += to_string(ingredient);
        }
        meal.meal = foodName;

        // We push the meal in the meals queue.
        meals.push(meal);
    }

    // And finally we notify the waiter, that will notify the customer, that will eat, and boom, we're done.
    mealPrepared.notify_one();
}

int main() {
    srand(static_cast<unsigned>(time(nullptr))); // Seed for randomness


    // Create all the threads
    thread waiterThread(waiter);
    thread cookThread(cook);
    thread chefThread(chef);
    thread customerThread(customer);

    // Make sure to join them, could probably detach all the threads beside customer threads.
    customerThread.join();
    chefThread.detach();
    cookThread.detach();
    waiterThread.detach();

    // Tadaaaa
    return 0;
}
