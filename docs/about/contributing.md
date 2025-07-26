# Contributing to Odyssey

---

We welcome contributions from everyone! Whether you're fixing a bug,
improving documentation, or suggesting new features, your help is
appreciated. Please read the guidelines below to get started and ensure
a smooth collaboration. Thank you for making Odyssey better!

## Reporting an issue

You can report issues on [GitHub](https://github.com/yandex/odyssey/issues).

Please include as much detail as you can.
Let us know your platform, execution environment and exact version of Odyssey
that you are using.

It is especially useful if an issue report covers all of the following points:

1. What are you trying to achieve?
2. What is your Odyssey configuration file
3. If possible, requests to the Odyssey that are meaningful
3. if possible, attach a script that reproduces your load profile.
4. Is Odyssey crashed, add traceback (`gdb odyssey -c corefile --batch -ex "t a a bt"`)
or whole core dump file.

## Getting Started With Code Contribution

See `Development` section for particular details about Odyssey code
changing and testing.

To get started with contributing to Odyssey, follow these steps:

1. Fork the [repository](https://github.com/yandex/odyssey) on GitHub.
2. Clone the forked repository to your local machine.
3. Create a new branch for your changes.
4. Make your changes and commit them.
5. Dont forget to run `make format` before pushing your changes.
6. Push your changes to your forked repository.
7. Submit a pull request to the [main repository](https://github.com/yandex/odyssey/pulls).
