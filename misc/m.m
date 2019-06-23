
function rv = heat_res_o(lambda,id,od,len)
  rv = log(od/id)/(2*pi*len*lambda);
endfunction


heat_cond_mirenol = 0.038; #[W/(mK)]
heat_cond_pex = 0.45;
heat_cond_mur_cp = 0.8;
heat_cond_beton = 1.3;
heat_cond_skvara = 0.27;

r_pex = heat_res_o(heat_cond_pex, 12, 16, 1)
r_iz = heat_res_o(heat_cond_mirenol, 16, 16+2*6, 1)
r_beton = 1*heat_res_o(heat_cond_beton, 16+2*6, 100, 1)
p = (80-20)/(r_pex + r_iz + r_beton)

