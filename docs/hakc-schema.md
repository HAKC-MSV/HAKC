<!-- @formatter:off -->

## DAG for Spatial Compartmentalization
```

┌────────┐      ┌──────────┐       ┌────────┐       
│        │      │          │       │        │       
│ Scope  │◄─────┤  Symbol  ├──────►│  Type  │       
│        │      │          │       │        │       
└────────┘      └────┬─────┘       └────────┘       
                     │                              
                     ▼                              
                 ┌────────┐                         
                 │        │                         
                 │  Div   │                         
                 │        │                         
                 └───┬────┘                         
                     │                              
                     ▼                              
                 ┌────────┐                         
                 │        │                         
                 │  Comp  │                         
                 │        │                         
                 └────────┘                         

```

<!-- @formatter:on -->
