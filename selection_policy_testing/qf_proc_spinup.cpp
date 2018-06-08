/** Stub code to be added to query_frontend_main.cpp. Very short. */
int main() {
    popen("redis-server", "r"); // These are the two lines of code we need to add.
    popen("python file_name.py", "r") //Replace file_name with actual python code
    // We continue with the designating the query processor as a listener.
}
