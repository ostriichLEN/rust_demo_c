use rayon::prelude::*;

fn main() {
    let mut per_source_sales = vec![0usize; 16];

    (0..100_000usize).into_par_iter().for_each(|order_id| {
        let source_id = order_id % 16;
        per_source_sales[source_id] += 1;
    });

    println!("{per_source_sales:?}");
}

