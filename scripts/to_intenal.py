'''
Creates the c-code to look for a clause.
'''


clause = "3 15 40 -59 76"

clause = clause.split()
clause = [int(x) for x in clause]

print(f"bool equals(struct clause *C) {{")
print(f"    literal_t literals[] = {{", end="")
for literal in clause:
    print(f"{(abs(literal) - 1) << 1 | int((literal < 0))}, ", end="")

print("\b\b", end="")
print("};")
print(f"    if (C->size != {len(clause)}) {{")
print(f"        return false;")
print(f"    }}")
print(f"    for (size_t i = 0; i < {len(clause)}); i++) {{")
print(f"        if (!literal_in_clause(literals[i], C)) {{")
print(f"            return false;")
print(f"        }}")
print(f"    }}")
print(f"    return true;")
print(f"}}")




