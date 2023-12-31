#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Structure to represent an atom in the S-expression tree
typedef struct {
  char *value;
  struct Atom **children;
  size_t numChildren;
} Atom;

bool isList(const Atom *atom) { return !strncmp(atom->value, "LIST", 4); }

// Function to create a new atom
Atom *createAtom(const char *value) {
  Atom *atom = (Atom *)zAlloc(sizeof(Atom));
  if (atom == NULL) {
    perror("Memory allocation error");
    exit(EXIT_FAILURE);
  }

  atom->value = strdup(value);
  atom->numChildren = 0;
  atom->children = NULL;

  return atom;
}

// Function to add a child to an atom
void addChild(Atom *parent, Atom *child) {
  parent->numChildren++;
  parent->children =
      (Atom **)zRealloc(parent->children, parent->numChildren * sizeof(Atom *));
  if (parent->children == NULL) {
    perror("Memory allocation error");
    exit(EXIT_FAILURE);
  }

  parent->children[parent->numChildren - 1] = child;
}

// Function to parse a list
Atom *parseList(FILE *stream) {
  Atom *list = createAtom("LIST");
  Atom *child;

  while ((child = parseSExpression(stream)) != NULL) {
    addChild(list, child);
  }

  return list;
}

// Function to parse an S-expression
Atom *parseSExpression(FILE *stream) {
  int c;
  while ((c = fgetc(stream)) != EOF) {
    switch (c) {
    case '(':
      return parseList(stream);
    case ')':
      fprintf(stderr, "Unexpected ')' character\n");
      exit(EXIT_FAILURE);
    case '\'': // Single quote indicates the beginning of a list
      return parseList(stream);
    case ' ':
    case '\t':
    case '\n':
      // Skip whitespace characters
      break;
    default:
      // Assume it's an atom
      // Read the atom until a whitespace or ')' character is encountered
      ungetc(c, stream); // Put back the character to start reading the atom
      char buffer[1024];
      int i = 0;
      while ((c = fgetc(stream)) != EOF && c != '(' && c != ')' && c != ' ' &&
             c != '\t' && c != '\n') {
        buffer[i++] = c;
      }
      buffer[i] = '\0';
      return createAtom(buffer);
    }
  }

  return NULL; // End of file
}
