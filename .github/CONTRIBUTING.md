# Contributing to PHL

Thank you for your interest in contributing to PHL! This document provides guidelines and information for contributors.

## Getting Started

### Prerequisites

- C compiler (GCC, Clang, or MSVC)
- Make
- Git

### Building the Project

```bash
# Clone the repository
git clone https://github.com/alganet/PHL.git
cd PHL

# Build
make

# Run tests
make test
```

## How to Contribute

### Reporting Issues

- Use GitHub Issues to report bugs or request features
- Provide detailed information including:
  - PHL version
  - Platform/OS
  - Steps to reproduce
  - Expected vs actual behavior

### Submitting Changes

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/your-feature-name`
3. Make your changes
4. Add tests if applicable
5. Ensure all tests pass: `make test`
6. Rebase your branch onto the latest main: `git rebase origin/main`
7. Commit your changes: `git commit -m "Description of changes"`
8. Push to your fork: `git push origin feature/your-feature-name`
9. Create a Pull Request

### Pull Request Guidelines

- Keep PRs focused on a single feature or bug fix
- Include a clear description of the changes
- Reference any related issues
- Ensure CI checks pass
- Follow the coding standards below

### Branch Strategy

- We maintain a linear main branch history
- Only rebase-style commits are accepted (no merge commits)
- Please rebase your feature branch onto the latest main before submitting a PR
- If your PR has multiple commits, they should be logical and well-organized
- Each individual commit should be able to pass tests and CI

## Coding Standards

### C Code Style

- Use ANSI C89/C90 compatible code
- Follow the existing code style in the project
- Use meaningful variable and function names
- Add comments for complex logic
- Keep functions small and focused

### Commit Messages

- Use clear, descriptive commit messages
- Keep the first line under 50 characters
- Add detailed description if needed

## Testing

- Add unit tests for new features
- Ensure existing tests still pass
- Test on multiple platforms if possible

## Documentation

- Update documentation for API changes
- Add comments to code for clarity
- Update examples if relevant

## License

By contributing to PHL, you agree that your contributions will be licensed under the BSD 3-Clause License.

## Questions?

If you have questions about contributing, feel free to open an issue or contact the maintainers.