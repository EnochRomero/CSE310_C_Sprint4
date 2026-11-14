# Pocket Budget Tracker

This project is a personal budget tracker that lets the user add income and expenses, review recent transactions, view totals, check the current balance, and remove entries. It uses a simple C++ web server to serve an HTML form and process financial data in memory.

## Instructions for Build and Use

Steps to build and/or run the software:

1. Install MSYS2 and the UCRT64 GCC C++ compiler on Windows.
2. Open PowerShell in the project folder and run:

```powershell
$env:Path = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;$env:Path"
& 'C:\msys64\ucrt64\bin\g++.exe' -std=c++17 main.cpp -o budget-tracker.exe -lws2_32
.\budget-tracker.exe
```

3. Open a browser and go to http://127.0.0.1:8080 to use the app. Press Ctrl+C in the terminal to stop the server.

Instructions for using the software:

1. Enter a description, amount, type, and category for a transaction.
2. Click "Add transaction" to add it to the list.
3. Review the balance, income, expenses, and recent entries on the page.
4. Click "Remove" next to any transaction to delete it.

## Development Environment

To recreate the development environment, you need the following software and/or libraries with the specified versions:

* Windows 10 or newer
* MSYS2 with the UCRT64 GCC toolchain
* C++17 compiler support
* A browser such as Chrome, Edge, or Firefox

## Useful Websites to Learn More

I found these websites useful in developing this software:

* [C++ Reference](https://en.cppreference.com/)
* [MSYS2](https://www.msys2.org/)
* [MDN Web Docs - HTML Forms](https://developer.mozilla.org/en-US/docs/Learn/Forms)

## Future Work

The following items I plan to fix, improve, and/or add to this project in the future:

* [ ] Save transactions to a file so data persists after the server closes
* [ ] Add categories with custom editing and filtering
* [ ] Improve input validation and error messages for invalid values


